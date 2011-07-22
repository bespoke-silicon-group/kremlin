#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "defs.h"
#include "udr.h"
#include "log.h"
#include "debug.h"
#include "table.h"
#include "GTable.h"
#include "kremlin_deque.h"
#include "hash_map.h"
#include "cregion.h"
#include "Vector.h"

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)

#define DS_ALLOC_SIZE   100     // used for static data structures

#define MAX_SRC_TSA_VAL	6

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))


// Global definitions
int __kremlin_level_to_log = -1;

// These are the levels on which we are ``reporting'' (i.e. writing info out to the
// .bin file)
unsigned int __kremlin_min_level = 0;
unsigned int __kremlin_max_level = 20;

// To be able to calculate SP, we need to profile one more level than we
// report
unsigned int __kremlin_max_profiled_level = 21;

char* __kremlin_output_filename;

extern int	__kremlin_debug;
extern int  __kremlin_debug_level;

UInt64*     getDynamicRegionId(UInt64 sid);
void        incDynamicRegionId(UInt64 sid);
UInt64      sidHash(UInt64 sid);
int         sidCompare(UInt64 s1, UInt64 s2);

typedef struct _CDT_T {
    UInt64* time;
    UInt32* version;
	UInt32 size;
} CDT;

typedef struct _FuncContext {
    LTable* table;
    TEntry* ret; // should this change to TSArray?
	UInt64 callSiteId;
#ifdef MANAGE_BB_INFO
    UInt    retBB;
    UInt    retPrevBB;
#endif
} FuncContext;

typedef struct _region_t {
    UInt64 start;
    UInt64 did;
    UInt64 cp;
    UInt64 regionId;
	UInt64 childrenWork;
	UInt64 childrenCP;
    RegionType regionType;
    UInt64 loadCnt;
    UInt64 storeCnt;
#ifdef EXTRA_STATS
    UInt64 readCnt;
    UInt64 writeCnt;
    UInt64 readLineCnt;
    UInt64 writeLineCnt;
#endif
} Region;

typedef struct _InvokeRecord {
    UInt64 id;
    int stackHeight;
} InvokeRecord;

#define isKremlinOn()       (kremlinOn == 1)
#define getCurrentLevel()  (levelNum)
#define getLevelOffset(x)	(x - __kremlin_min_level)
//#define getLevelOffset(x)	(x)

#define isCurrentLevelInstrumentable() (((levelNum) >= __kremlin_min_level) && ((levelNum) <= __kremlin_max_profiled_level))

void initStartFuncContext();
HASH_MAP_DEFINE_PROTOTYPES(sid_did, UInt64, UInt64);
HASH_MAP_DEFINE_FUNCTIONS(sid_did, UInt64, UInt64);

// A vector used to represent the call stack.
VECTOR_DEFINE_PROTOTYPES(FuncContexts, FuncContext*);
VECTOR_DEFINE_FUNCTIONS(FuncContexts, FuncContext*, VECTOR_COPY, VECTOR_NO_DELETE);

// A vector used to record invoked calls.
VECTOR_DEFINE_PROTOTYPES(InvokeRecords, InvokeRecord);
VECTOR_DEFINE_FUNCTIONS(InvokeRecords, InvokeRecord, VECTOR_COPY, VECTOR_NO_DELETE);

int                 kremlinOn = 0;
int                 levelNum = 0;
UInt32				storageSize = 0;
int					numActiveLevels = 0; // number of levels currently being logged

TSArray**			src_arrays = NULL; // 2D array of TSArray*
int					src_arrays_size = 0;
TSArray*			dest_arrays = NULL; // array of TSArray*
int					dest_arrays_size = 0;

int*                versions = NULL;
Region*             regionInfo = NULL;
CDT*                cdtHead = NULL;
FuncContexts*       funcContexts;
InvokeRecords*      invokeRecords;
UInt64              timestamp = 0llu;
UInt64              loadCnt = 0llu;
UInt64              storeCnt = 0llu;
File*               fp = NULL;
deque*              argTimestamps;
hash_map_sid_did*   sidToDid;
UInt64              lastCallSiteId;
UInt64              calledRegionId;
GTable*             gTable;

#ifdef MANAGE_BB_INFO
UInt    __prevBB;
UInt    __currentBB;
#endif


UInt64 _regionFuncCnt;
UInt64 _setupTableCnt;
int _requireSetupTable;

/**
 * start profiling
 *
 * push the root region (id == 0, type == loop)
 * loop type seems weird, but using function type as the root region
 * causes several problems regarding local table
 *
 * when kremlinOn == 0,
 * most instrumentation functions do nothing.
 */ 
void turnOnProfiler() {
    kremlinOn = 1;
    logRegionEntry(0, 1);
	fprintf(stderr, "[kremlin] Logging started.\n");
}

/**
 * end profiling
 *
 * pop the root region pushed in turnOnProfiler()
 */
void turnOffProfiler() {
    logRegionExit(0, 1);
    kremlinOn = 0;
	fprintf(stderr, "[kremlin] Logging stopped.\n");
}

void pauseProfiler() {
	kremlinOn = 0;
	fprintf(stderr, "[kremlin] Logging paused.\n");
}

void resumeProfiler() {
	kremlinOn = 1;
	fprintf(stderr, "[kremlin] Logging resumed.\n");
}
int _maxRegionNum = 0;


TEntry* dummyEntry = NULL;

void allocDummyTEntry() { dummyEntry = allocTEntry(); }

TEntry* getDummyTEntry() { return dummyEntry; }

void freeDummyTEntry() {
    //freeTEntry(dummyEntry);
	free(dummyEntry); // dummy will always have NULL for time/version
    dummyEntry = NULL;
}

void prepareCall(UInt64 callSiteId, UInt64 calledRegionId) {
    if(!isKremlinOn()) { return; }

    // Clear off any argument timestamps that have been left here before the
    // call. These are left on the deque because library calls never take
    // theirs off. 
    deque_clear(argTimestamps);
	lastCallSiteId = callSiteId;
}

void linkArgToLocal(UInt src) {
    if (!isKremlinOn())
        return;
    MSG(1, "linkArgToLocal to ts[%u]\n", src);

#ifndef WORK_ONLY
    deque_push_back(argTimestamps, getLTEntry(src));
#endif
}

// special case for constant arg
void linkArgToConst() {
    if (!isKremlinOn())
        return;

    MSG(1, "linkArgToConst\n");

#ifndef WORK_ONLY
    deque_push_back(argTimestamps, getDummyTEntry());
#endif
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(UInt dest) {
    if (!isKremlinOn())
        return;
    MSG(1, "transfer arg data to ts[%u]\n", dest);

#ifndef WORK_ONLY
    TEntry* destEntry = getLTEntry(dest);
    TEntry* srcEntry = deque_pop_front(argTimestamps);
    assert(destEntry);

    // If no arg was passed, the use the dummy one
    // library functions will not pass arguments, so we do not know their
    // timestamps.
    if(!srcEntry)
        srcEntry = getDummyTEntry();

    copyTEntry(destEntry, srcEntry);
#endif
}


/**
 * Setup the local shadow register table.
 * @param maxVregNum	Number of virtual registers to allocate.
 */
void setupLocalTable(UInt maxVregNum) {
    if(!isKremlinOn()) { return; }

    MSG(1, "setupLocalTable size %u\n", maxVregNum);

#ifndef WORK_ONLY
    assert(_requireSetupTable == 1);

    LTable* table = allocLocalTable(maxVregNum);
    assert(table != NULL);

    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    assert(funcHead->table == NULL);

    funcHead->table = table;
    setLocalTable(funcHead->table);
    _setupTableCnt++;
    _requireSetupTable = 0;
#endif
}

void incrementRegionLevel() {
    levelNum++;
    _maxRegionNum = MAX(_maxRegionNum, levelNum);
}

void decrementRegionLevel() { levelNum--; }

/**
 * Increments the dynamic id count for a static region.
 *
 * @param sid       The static id.
 */
void incDynamicRegionId(UInt64 sid) {
    UInt64* did = getDynamicRegionId(sid);
    (*did)++;
}

/**
 * Returns a pointer to the dynamic id count.
 *
 * @param sid       The static id.
 * @return          The dynamic id count.
 */
UInt64* getDynamicRegionId(UInt64 sid) {
    UInt64* did;
    if(!(did = hash_map_sid_did_get(sidToDid, sid))) {
        hash_map_sid_did_put(sidToDid, sid, 0, TRUE);
        did = hash_map_sid_did_get(sidToDid, sid);
    }
    assert(did);
    return did;
}

/**
 * Returns timestamp of control dep at specified level with specified version.
 * @param level 	Level at which to look for control dep.
 * @param version	Version we are looking for.
 * @return			Timestamp of control dep.
 */
// preconditions: cdtHead != NULL && level >= 0
UInt64 getCdt(int level, UInt32 version) {
    //assert(cdtHead != NULL);
    return (cdtHead->version[level - __kremlin_min_level] == version) ?
        cdtHead->time[level - __kremlin_min_level] : 0;
}

/**
 * Sets the control dep at specified level to specified (version,time) pair
 * @param level		Level to set.
 * @param version	Version number to set.
 * @param time		Timestamp to set control dep to.
 */
void setCdt(int level, UInt32 version, UInt64 time) {
    assert(level >= __kremlin_min_level);
    cdtHead->time[level - __kremlin_min_level] = time;
    cdtHead->version[level - __kremlin_min_level] = version;
}

/**
 * Copies data from table entry over to cdt.
 * @param cdt		Pointer to CDT to copy data to.
 * @param entry		Pointer to TEntry containing data to be copied.
 */
void fillCDT(CDT* cdt, TEntry* entry) {
	// entry may not have data for all logged levels so we have to check timeArrayLength
    int i;
    for (i = __kremlin_min_level; i <= __kremlin_max_profiled_level; ++i) {
        int level = i - __kremlin_min_level;

		if(i < entry->timeArrayLength) {
        	cdt->time[level] = entry->time[level];
        	cdt->version[level] = entry->version[level];
		}
		else {
        	cdt->time[level] = 0;
        	cdt->version[level] = 0;
		}
    }
}

/**
 * Pushes new context onto function context stack.
 */
void pushFuncContext() {
    FuncContext* funcContext = (FuncContext*) malloc(sizeof(FuncContext));
    assert(funcContext);

    FuncContextsPushVal(funcContexts, funcContext);
    funcContext->table = NULL;
	funcContext->callSiteId = lastCallSiteId;
	funcContext->ret = NULL;

#ifdef MANAGE_BB_INFO
    funcContext->retBB = __currentBB;
    funcContext->retPrevBB = __prevBB;
    MSG(1, "[push] current = %u last = %u\n", __currentBB, __prevBB);
#endif
	//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);

}

/**
 * Prints out TEntry times.
 * @param entry		Pointer to TEntry.
 * @param size		Number of entries to print
 */
void dumpTEntry(TEntry* entry, int size) {
    int i;
    fprintf(stderr, "entry@%p\n", entry);
	// XXX: assert on bounds check?
    for (i = 0; i < size; i++) {
        fprintf(stderr, "\t%llu", entry->time[i]);
    }
    fprintf(stderr, "\n");
}

/**
 * Updates (and returns) critical path length of region at specified level. If specified timestamp
 * is greater than current CP length, CP length is updated with this time.
 * @param value			Timestamp to update with.
 * @param level			Region level to update.
 * @return				Critical path length of specified level.
 */
UInt64 updateCP(UInt64 value, int level) {
	int true_level = level - __kremlin_min_level;
	//int true_level = level;
	regionInfo[true_level].cp = MAX(value,regionInfo[true_level].cp);
	return regionInfo[true_level].cp;

}

#define addWork(work) timestamp += work;

void addLoad() { loadCnt++; }
void addStore() { storeCnt++; }

/**
 * Removes context at the top of the function context stack.
 */
void popFuncContext() {
    FuncContext* ret = FuncContextsPopVal(funcContexts);
    assert(ret);
    //assert(ret->table != NULL);
#ifdef MANAGE_BB_INFO
    // restore currentBB and prevBB
    __currentBB = ret->retBB;
    __prevBB = ret->retPrevBB;
#endif

    assert(_regionFuncCnt == _setupTableCnt);
    assert(_requireSetupTable == 0);

    if (ret->table != NULL)
        freeLocalTable(ret->table);

    free(ret);  
}

void fillTSArray(TEntry* src_entry, TSArray* dest_tsa) {
	int fill_size = MIN(src_entry->timeArrayLength,numActiveLevels);

	// Fill in the data, taking into account version
	int i;
	for(i = 0; i < fill_size; ++i) {
    	dest_tsa->times[i] = (src_entry->version[i] == versions[i]) ?  src_entry->time[i] : 0;
	}

	// TODO: memset for this?
	for(i = fill_size; i < numActiveLevels; ++i) { dest_tsa->times[i] = 0; }
}

/*
 * Filles tsa array with timestamps associated with vreg
 * @param vreg			Virtual register number to get data from
 * @param tsa			TSArray to write TS data to
 */
void getLocalTimes(UInt src_vreg, TSArray* tsa) {
	TEntry* entry = getLTEntry(src_vreg);

	fillTSArray(entry, tsa);
}

void getGlobalTimes(Addr src_addr, TSArray* tsa) {
    TEntry* entry = GTableGetTEntry(gTable, src_addr);

	fillTSArray(entry, tsa);
}

void getCDTTSArray(TSArray* tsa) {
	int i;
	for(i = 0; i < numActiveLevels; ++i) {
		tsa->times[i] = (cdtHead->version[i] == versions[i]) ?  cdtHead->time[i] : 0;
	}
}

// precondition: numActiveLevels > 0
TSArray* getSrcTSArray(UInt idx) { 
	assert(idx <= MAX_SRC_TSA_VAL);

	// TODO: move this if to logRegionEntry for more efficiency
	if(src_arrays_size < numActiveLevels) {
		src_arrays = realloc(src_arrays,sizeof(TSArray*)*numActiveLevels);

		for( ; src_arrays_size < numActiveLevels; ++src_arrays_size) {
			src_arrays[src_arrays_size] = malloc(sizeof(TSArray)*MAX_SRC_TSA_VAL);

			int i;
			for(i = 0; i < MAX_SRC_TSA_VAL; ++i) {
				src_arrays[src_arrays_size][i].times =
					malloc(sizeof(Timestamp)*(src_arrays_size+1));
				src_arrays[src_arrays_size][i].size = src_arrays_size+1;
			}
		}
	}

	return &src_arrays[numActiveLevels-1][idx];
}

// precondition: numActiveLevels > 0
TSArray* getDestTSArray() { 
	// TODO: move this if to logRegionEntry for more efficiency
	if(dest_arrays_size < numActiveLevels) {
		dest_arrays = realloc(dest_arrays,sizeof(TSArray)*numActiveLevels);

		for( ; dest_arrays_size < numActiveLevels; ++dest_arrays_size) {
			dest_arrays[dest_arrays_size].times = 
				malloc(sizeof(Timestamp)*(dest_arrays_size+1));
			dest_arrays[dest_arrays_size].size = dest_arrays_size+1;
		}
	}

	return &dest_arrays[numActiveLevels-1];
}

void updateTEntry(TEntry* entry, TSArray* src_tsa) {
    TEntryAllocAtLeastLevel(entry, 0);

	int i;
	for(i = 0; i < numActiveLevels; ++i) {
		entry->time[i] = src_tsa->times[i];
		regionInfo[i].cp = MAX(src_tsa->times[i],regionInfo[i].cp); // update CP

		entry->version[i] = versions[i];
	}
}

void updateLocalTimes(UInt dest_vreg, TSArray* src_tsa) {
	TEntry* entry = getLTEntry(dest_vreg);

	updateTEntry(entry, src_tsa);
}

void updateGlobalTimes(Addr dest_addr, TSArray* src_tsa) {
    TEntry* entry = GTableGetTEntry(gTable, dest_addr);

	updateTEntry(entry, src_tsa);
}

/*
 * Returns version ID at specified region level.
 * @param level			Region level to get version from.
 * @return				Version ID at specified level.
 */
UInt getVersion(int level) {
    assert(level >= 0 && level < DS_ALLOC_SIZE);
    return versions[level];
}

// precondition: inLevel >= __kremlin_min_level && inLevel < maxRegionSize
inline UInt64 getTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
    int level = inLevel - __kremlin_min_level;

    UInt64 ret = (level >= 0 && level < entry->timeArrayLength && entry->version[level] == version) ?
                    entry->time[level] : 0;
    return ret;
}


// precondition: entry != NULL
inline void updateTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int level = inLevel - __kremlin_min_level;
    entry->version[level] = version;
    entry->time[level] = timestamp;
}

#ifdef EXTRA_STATS
UInt64 getReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
    int level = inLevel - __kremlin_min_level;
    assert(entry != NULL);
    return (level >= 0 && entry->timeArrayLength > level && entry->readVersion[level] == version) ?
                    entry->readTime[level] : 0;
}

void updateReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int level = inLevel - __kremlin_min_level;
    assert(entry != NULL);

    entry->readVersion[level] = version;
    entry->readTime[level] = timestamp;
}

void updateReadMemoryAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getReadTimestamp(entry, inLevel, version);

    if (prevTimestamp == 0LL) {
        regionInfo[region].readCnt++;
        //fprintf(stderr, "\t[load] addr = 0x%x level = %d version = %d timestamp = %d\n",
        //  entry, inLevel, version, timestamp);

        updateReadTimestamp(entry, inLevel, version, timestamp);
    }
}
    
// 1) update readCnt and writeCnt
// 2) update timestamp
void updateWriteMemoryAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].writeCnt++;
    }   
    //updateTimestamp(entry, inLevel, version, timestamp);
}

void updateReadMemoryLineAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getReadTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].readLineCnt++;
        //fprintf(stderr, "[line] addr = 0x%x level = %d version = %d timestamp = %d\n",
        //  entry, inLevel, version, timestamp);
        updateReadTimestamp(entry, inLevel, version, timestamp);

    }
}

void updateWriteMemoryLineAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].writeLineCnt++;
    }
    updateTimestamp(entry, inLevel, version, timestamp);
}
#endif

void prepareInvoke(UInt64 id) {
    if(!isKremlinOn())
        return;

    MSG(1, "prepareInvoke(%llu) - saved at %lld\n", id, (UInt64)getCurrentLevel());
   
    InvokeRecord* currentRecord = InvokeRecordsPush(invokeRecords);
    currentRecord->id = id;
    currentRecord->stackHeight = getCurrentLevel();
}

void invokeOkay(UInt64 id) {
    if(!isKremlinOn())
        return;

    if(!InvokeRecordsEmpty(invokeRecords) && InvokeRecordsLast(invokeRecords)->id == id) {
        MSG(1, "invokeOkay(%u)\n", id);
        InvokeRecordsPop(invokeRecords);
    } else
        MSG(1, "invokeOkay(%u) ignored\n", id);
}
void invokeThrew(UInt64 id)
{
    if(!isKremlinOn())
        return;

    fprintf(stderr, "invokeRecordOnTop: %u\n", InvokeRecordsLast(invokeRecords)->id);

    if(!InvokeRecordsEmpty(invokeRecords) && InvokeRecordsLast(invokeRecords)->id == id) {
        InvokeRecord* currentRecord = InvokeRecordsLast(invokeRecords);
        MSG(1, "invokeThrew(%u) - Popping to %d\n", currentRecord->id, currentRecord->stackHeight);
        while(getCurrentLevel() > currentRecord->stackHeight)
        {
            UInt64 lastLevel = getCurrentLevel();
            Region* region = regionInfo + getLevelOffset(getCurrentLevel());
            logRegionExit(region->regionId, region->regionType);
            assert(getCurrentLevel() < lastLevel);
            assert(getCurrentLevel() >= 0);
        }
        InvokeRecordsPop(invokeRecords);
    }
    else
        MSG(1, "invokeThrew(%u) ignored\n", id);
}

void initCurrentRegion(UInt64 regionId, UInt64 did, UInt regionType) {
    int curr_level = getCurrentLevel();

    regionInfo[getLevelOffset(curr_level)].regionId = regionId;
    regionInfo[getLevelOffset(curr_level)].start = timestamp;
    regionInfo[getLevelOffset(curr_level)].did = did;
    regionInfo[getLevelOffset(curr_level)].cp = 0LL;
    regionInfo[getLevelOffset(curr_level)].childrenWork = 0LL;
    regionInfo[getLevelOffset(curr_level)].childrenCP = 0LL;
    regionInfo[getLevelOffset(curr_level)].regionType = regionType;

    regionInfo[getLevelOffset(curr_level)].loadCnt = 0LL;
    regionInfo[getLevelOffset(curr_level)].storeCnt = 0LL;
#ifdef EXTRA_STATS
    regionInfo[getLevelOffset(curr_level)].readCnt = 0LL;
    regionInfo[getLevelOffset(curr_level)].writeCnt = 0LL;
    regionInfo[getLevelOffset(curr_level)].readLineCnt = 0LL;
    regionInfo[getLevelOffset(curr_level)].writeLineCnt = 0LL;
#endif
}

void logRegionEntry(UInt64 regionId, UInt regionType) {
    if (!isKremlinOn()) { return; }

    if(regionType == RegionFunc)
    {
		_regionFuncCnt++;
        pushFuncContext();
        _requireSetupTable = 1;
    }

    incrementRegionLevel();

    int curr_level = getCurrentLevel();

#ifndef USE_UREGION
    FuncContext* funcHead = *FuncContextsLast(funcContexts);
	UInt64 callSiteId = (funcHead == NULL) ? 0x0 : funcHead->callSiteId;
	cregionPutContext(regionId, callSiteId);
#endif

	if(isCurrentLevelInstrumentable()) numActiveLevels++;

	// If we exceed the maximum depth, we act like this region doesn't exist
	if(curr_level > __kremlin_max_profiled_level) { return; }


    incDynamicRegionId(regionId);
    versions[curr_level]++;

    if (regionType < RegionLoopBody)
        MSG(0, "[+++] region [%u, %d, %llu:%llu] start: %llu\n",
            regionType, curr_level, regionId, *getDynamicRegionId(regionId), timestamp);

	// set initial values for newly entered region (but make sure we are only
	// doing this for logged levels)
	if(curr_level >= __kremlin_min_level) {
		initCurrentRegion(regionId,*getDynamicRegionId(regionId),regionType);
	}


#ifndef WORK_ONLY
	if(isCurrentLevelInstrumentable())
        setCdt(curr_level, versions[curr_level], 0);
#endif

    incIndentTab(); // only affects debug printing
}

/**
 * Does the clean up work when exiting a function region.
 */
void handleFuncRegionExit() {
	popFuncContext();

	FuncContext* funcHead = *FuncContextsLast(funcContexts);

	if (funcHead == NULL) { assert(getCurrentLevel() == 0); }
	else { setLocalTable(funcHead->table); }

#if MANAGE_BB_INFO
	MSG(1, "    currentBB: %u   lastBB: %u\n",
		__currentBB, __prevBB);
#endif
}


/**
 * Creates RegionField and fills it based on inputs.
 */
RegionField fillRegionField(UInt64 work, UInt64 cp, UInt64 callSiteId, UInt64 spWork, UInt64 tpWork, Region region_info) {
	RegionField field;

    field.work = work;
    field.cp = cp;
	field.callSite = callSiteId;
	field.spWork = spWork;
	field.tpWork = tpWork;

    field.loadCnt = region_info.loadCnt;
    field.storeCnt = region_info.storeCnt;
#ifdef EXTRA_STATS
    field.readCnt = region_info.readCnt;
    field.writeCnt = region_info.writeCnt;
    field.readLineCnt = region_info.readLineCnt;
    field.writeLineCnt = region_info.writeLineCnt;
    assert(work >= field.readCnt && work >= field.writeCnt);
#endif

	return field;
}


/**
 * Handles exiting of regions. This includes handling function context, calculating profiled
 * statistics, and logging region statistics.
 * @param regionID		ID of region that is being exited.
 * @param regionType	Type of region being exited.
 */
void logRegionExit(UInt64 regionId, UInt regionType) {
    if (!isKremlinOn()) { return; }

    int curr_level = getCurrentLevel();

	if(isCurrentLevelInstrumentable()) numActiveLevels--;

	// If we are outside range of levels, handle function stack then exit
	if(!isCurrentLevelInstrumentable()) {
#ifndef WORK_ONLY
		if (regionType == RegionFunc) { handleFuncRegionExit(); }
#endif
    	decrementRegionLevel();
    	decIndentTab(); // applies only to debug printing
#ifndef USE_UREGION
		cregionRemoveContext(NULL);
#endif
		return;
	}

	Region curr_region = regionInfo[getLevelOffset(curr_level)];

    UInt64 sid = regionId;
    UInt64 did = curr_region.did;
    if(curr_region.regionId != regionId) {
		fprintf(stderr, "mismatch in regionID. expected %llu, got %llu. curr_level = %d\n",curr_region.regionId,regionId,curr_level);
		assert(0);
	}

    UInt64 work = timestamp - curr_region.start;
    UInt64 cp = curr_region.cp;

	UInt64 parentSid = 0;
	UInt64 parentDid = 0;

	// Only update parent region's childrenWork and childrenCP when we are
	// logging the parent
	if (curr_level > __kremlin_min_level) {
		Region parent_region = regionInfo[getLevelOffset(curr_level)-1];

    	parentSid = parent_region.regionId;
		parentDid = parent_region.did;
		parent_region.childrenWork += work;
		parent_region.childrenCP += cp;
	} 

    if (regionType < RegionLoopBody)
        MSG(0, "[---] region [%u, %u, %llu:%llu] cp %llu work %llu\n",
                regionType, curr_level, regionId, did, curr_region.cp, work);

	assert(!isCurrentLevelInstrumentable() || work == 0 || cp > 0);
	assert(work >= cp);
	assert(work >= curr_region.childrenWork);


	// Check that cp is positive if work is positive.
	// This only applies when the current level gets instrumented (otherwise this condition always holds)
    if (cp == 0 && work > 0 && isCurrentLevelInstrumentable()) {
        fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
        fprintf(stderr, "region [type: %u, curr_level: %u, id: %llu:%llu] parent [%llu:%llu] cp %llu work %llu\n",
            regionType, curr_level, regionId, did, parentSid, parentDid, 
            curr_region.cp, work);
        assert(0);
    }

	double spTemp = (work - curr_region.childrenWork + curr_region.childrenCP) / (double)cp;
	double sp = (work > 0) ? spTemp : 1.0;

	if(sp < 1.0) {
		fprintf(stderr, "sid=%lld work=%llu childrenWork = %llu childrenCP=%lld\n", sid, work,
			curr_region.childrenWork, curr_region.childrenCP);
		assert(0);
	}

	UInt64 spWork = (UInt64)((double)work / sp);
	UInt64 tpWork = cp;

	// due to floating point variables,
	// spWork or tpWork can be larger than work
	if (spWork > work) { spWork = work; }
	if (tpWork > work) { tpWork = work; }

    decIndentTab(); // applies only to debug printing

    /*
    if (regionType < RegionLoopBody)
        MSG(0, "[---] region [%u, %u, %llu:%llu] parent [%llu:%llu] cp %llu work %llu\n",
                regionType, curr_level, regionId, did, parentSid, parentDid, 
                curr_region.cp, work);
    */

    RegionField field = fillRegionField(work,cp,(*FuncContextsLast(funcContexts))->callSiteId,spWork,tpWork,curr_region);

#ifdef USE_UREGION
    processUdr(sid, did, parentSid, parentDid, field);
#else
	cregionRemoveContext(&field);
#endif
        
#ifndef WORK_ONLY
    if (regionType == RegionFunc) { handleFuncRegionExit(); }
#endif

    decrementRegionLevel();
}


void* logReductionVar(UInt opCost, UInt dest) {
    addWork(opCost);
    return NULL;
}

void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "binOp ts[%u] = max(ts[%u], ts[%u]) + %u\n", dest, src0, src1, opCost);
    addWork(opCost);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src0_tsa = getSrcTSArray(0);
	TSArray* src1_tsa = getSrcTSArray(1);

	getLocalTimes(src0, src0_tsa);
	getLocalTimes(src1, src1_tsa);

	TSArray* cdt_tsa = getSrcTSArray(2);
	
	getCDTTSArray(cdt_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		Timestamp max_src_ts = MAX(src0_tsa->times[i],src1_tsa->times[i]);
		dest_tsa->times[i] = MAX(max_src_ts,cdt_tsa->times[i]) + opCost;
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entry0 = getLTEntry(src0);
    TEntry* entry1 = getLTEntry(src1);
    TEntry* entryDest = getLTEntry(dest);

    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    TEntryAllocAtLeastLevel(entryDest, maxLevel);

    int i;
    for (i = minLevel; i <= maxLevel; ++i) {
		// calculate availability time
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);
        UInt64 ts0 = getTimestamp(entry0, i, version);
        UInt64 ts1 = getTimestamp(entry1, i, version);
        UInt64 greater0 = (ts0 > ts1) ? ts0 : ts1;
        UInt64 greater1 = (cdt > greater0) ? cdt : greater0;
        UInt64 value = greater1 + opCost;
        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
		
        MSG(2, "binOp[%u] level %u version %u \n", opCost, i, version);
        MSG(2, " src0 %u src1 %u dest %u\n", src0, src1, dest);
        MSG(2, " ts0 %u ts1 %u cdt %u value %u\n", ts0, ts1, cdt, value);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

void* logBinaryOpConst(UInt opCost, UInt src, UInt dest) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "binOpConst ts[%u] = ts[%u] + %u\n", dest, src, opCost);
    addWork(opCost);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	getLocalTimes(src, src_tsa);

	TSArray* cdt_tsa = getSrcTSArray(1);
	
	getCDTTSArray(cdt_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		dest_tsa->times[i] = MAX(src_tsa->times[i],cdt_tsa->times[i]) + opCost;
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entry0 = getLTEntry(src);
    TEntry* entryDest = getLTEntry(dest);
    
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    TEntryAllocAtLeastLevel(entryDest, maxLevel);

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);
        UInt64 ts0 = getTimestamp(entry0, i, version);
        UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
        UInt64 value = greater1 + opCost;

		// update time and critical path info
        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);

        MSG(2, "binOpConst[%u] level %u version %u \n", opCost, i, version);
        MSG(2, " src %u dest %u\n", src, dest);
        MSG(2, " ts0 %u cdt %u value %u\n", ts0, cdt, value);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}


void* logAssignment(UInt src, UInt dest) {
    if (!isKremlinOn()) return NULL;
    
    return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "logAssignmentConst ts[%u]\n", dest);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* cdt_tsa = getSrcTSArray(1);
	getCDTTSArray(cdt_tsa);

	updateLocalTimes(dest,cdt_tsa);

    return cdt_tsa;

	/*
    TEntry* entryDest = getLTEntry(dest);
    
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    int i;
    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);

        updateTimestamp(entryDest, i, version, cdt);
        updateCP(cdt, i);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

void* logLoadInst(Addr src_addr, UInt dest) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "load ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOAD_COST);
    addWork(LOAD_COST);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	getGlobalTimes(src_addr, src_tsa);

	TSArray* cdt_tsa = getSrcTSArray(1);
	getCDTTSArray(cdt_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		dest_tsa->times[i] = MAX(src_tsa->times[i],cdt_tsa->times[i]) + LOAD_COST;
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entry0 = GTableGetTEntry(gTable, src_addr);
    TEntry* entryDest = getLTEntry(dest);

    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

#ifdef EXTRA_STATS
    TEntryAllocAtLeastLevel(entry0, maxLevel);
#endif

    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    int i;
    for (i = minLevel; i <= maxLevel; i++) {
        regionInfo[getLevelOffset(i)].loadCnt++;
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);
        UInt64 ts0 = getTimestamp(entry0, i, version);
        UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
        UInt64 value = greater1 + LOAD_COST;

#ifdef EXTRA_STATS
        updateReadMemoryAccess(entry0, i, version, value);
#endif

        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

void* logLoadInst1Src(Addr src_addr, UInt src1, UInt dest) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "load ts[%u] = max(ts[0x%x],ts[%u]) + %u\n", dest, src_addr, src1, LOAD_COST);
    addWork(LOAD_COST);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* addr_tsa = getSrcTSArray(0);
	TSArray* src_tsa = getSrcTSArray(1);

	getGlobalTimes(src_addr, addr_tsa);
	getLocalTimes(src1, src_tsa);

	TSArray* cdt_tsa = getSrcTSArray(2);
	getCDTTSArray(cdt_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		Timestamp tmp_max_ts = MAX(addr_tsa->times[i],src_tsa->times[i]);
		dest_tsa->times[i] = MAX(tmp_max_ts,cdt_tsa->times[i]) + LOAD_COST;
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entrySrcAddr = GTableGetTEntry(gTable, src_addr);
    TEntry* entrySrc1 = getLTEntry(src1);
    TEntry* entryDest = getLTEntry(dest);

    assert(entrySrcAddr != NULL);
    assert(entrySrc1 != NULL);
    assert(entryDest != NULL);
    
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    int i;
    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);

        UInt64 cdt = getCdt(i,version);

        UInt64 ts_src_addr = getTimestamp(entrySrcAddr, i, version);
        UInt64 ts_src1 = getTimestamp(entrySrc1, i, version);

        UInt64 max1 = (ts_src_addr > cdt) ? ts_src_addr : cdt;
        UInt64 max2 = (max1 > ts_src1) ? max1 : ts_src1;

		UInt64 value = max2 + LOAD_COST;

        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);

        MSG(2, "logLoadInst1Src level %u version %u \n", i, version);
        MSG(2, " src_addr 0x%x src1 %u dest %u\n", src_addr, src1, dest);
        MSG(2, " cdt %u ts_src_addr %u ts_src1 %u max %u\n", cdt, ts_src_addr, ts_src1, max2);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

// TODO: implement these
void* logLoadInst2Src(Addr src_addr, UInt src1, UInt src2, UInt dest) { return logLoadInst(src_addr,dest); }
void* logLoadInst3Src(Addr src_addr, UInt src1, UInt src2, UInt src3, UInt dest) { return logLoadInst(src_addr,dest); }
void* logLoadInst4Src(Addr src_addr, UInt src1, UInt src2, UInt src3, UInt src4, UInt dest) { return logLoadInst(src_addr,dest); }


void* logStoreInst(UInt src, Addr dest_addr) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "store ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);

    addWork(STORE_COST);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	getLocalTimes(src, src_tsa);

	TSArray* cdt_tsa = getSrcTSArray(1);
	getCDTTSArray(cdt_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		dest_tsa->times[i] = MAX(src_tsa->times[i],cdt_tsa->times[i]) + STORE_COST;
	}

	updateGlobalTimes(dest_addr,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entry0 = getLTEntry(src);
    TEntry* entryDest = GTableGetTEntry(gTable, dest_addr);

    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    //fprintf(stderr, "\n\nstore ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);
    int i;
    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    //TEntryAllocAtLeastLevel(entryLine, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        regionInfo[getLevelOffset(i)].storeCnt++;
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);
        UInt64 ts0 = getTimestamp(entry0, i, version);
        UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
        UInt64 value = greater1 + STORE_COST;

#ifdef EXTRA_STATS
        updateWriteMemoryAccess(entryDest, i, version, value);
      //  updateWriteMemoryLineAccess(entryLine, i, version, value);
#endif

        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}


void* logStoreInstConst(Addr dest_addr) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "storeConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    addWork(STORE_COST);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* cdt_tsa = getSrcTSArray(0);
	getCDTTSArray(cdt_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		dest_tsa->times[i] = cdt_tsa->times[i] + STORE_COST;
	}

	updateGlobalTimes(dest_addr,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entryDest = GTableGetTEntry(gTable, dest_addr);
    assert(entryDest != NULL);

#ifdef EXTRA_STATS
    TEntry* entryLine = getGTEntryCacheLine(dest_addr);
#endif
    
    //fprintf(stderr, "\nstoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    int i;
    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; ++i) {
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);
        UInt64 value = cdt + STORE_COST;
#ifdef EXTRA_STATS
        updateWriteMemoryAccess(entryDest, i, version, value);
        updateWriteMemoryLineAccess(entryLine, i, version, value);
#endif
        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

// FIXME: support 64 bit address
// TODO: implement for new shadow mem interface
void logMalloc(Addr addr, size_t size, UInt dest) {
    if (!isKremlinOn()) return;
    
    MSG(1, "logMalloc addr=0x%x size=%llu\n", addr, (UInt64)size);

#ifndef WORK_ONLY

    // Don't do anything if malloc returned NULL
    if(!addr) { return; }

    createMEntry(addr,size);
#endif
}

// TODO: implement for new shadow mem interface
void logFree(Addr addr) {
    if (!isKremlinOn()) return;

    MSG(1, "logFree addr=0x%x\n", addr);

    // Calls to free with NULL just return.
    if(addr == NULL) return;


#ifndef WORK_ONLY
    size_t mem_size = getMEntry(addr);
    Addr a;
    for(a = addr; a < addr + mem_size; a++)
        GTableDeleteTEntry(gTable, a);

    freeMEntry(addr);

	addWork(FREE_COST);

	// make sure CP is at least the time needed to complete the free
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 value = getCdt(i,version) + FREE_COST;

        updateCP(value, i);
    }
#endif
}

// TODO: more efficient implementation (if old_addr = new_addr)
// XXX: This is wrong. Values in the realloc'd location should still have the
// same timestamp.
void logRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest) {
    if (!isKremlinOn())
        return;

    MSG(1, "logRealloc old_addr=0x%x new_addr=0x%x size=%llu\n", old_addr, new_addr, (UInt64)size);
    logFree(old_addr);
    logMalloc(new_addr,size,dest);
}

void* logInsertValue(UInt src, UInt dest) {
    //printf("Warning: logInsertValue not correctly implemented\n");
    return logAssignment(src, dest);
}

void* logInsertValueConst(UInt dest) {
    //printf("Warning: logInsertValueConst not correctly implemented\n");
    return logAssignmentConst(dest);
}

#define CDTSIZE	256
static CDT* cdtPool;
static int cdtSize;
static int cdtIndex;

CDT* allocCDT() {
	CDT* ret = &cdtPool[cdtIndex];
	cdtIndex++;
	if (cdtIndex == cdtSize) {
		int i;
		cdtSize += CDTSIZE;
		cdtPool = realloc(cdtPool, sizeof(CDT) * cdtSize);
		for (i=cdtSize-CDTSIZE; i<cdtSize; i++) {
    		cdtPool[i].time = (UInt64*) calloc(storageSize, sizeof(UInt64));
		    cdtPool[i].version = (UInt32*) calloc(storageSize, sizeof(UInt32));
			cdtPool[i].size = storageSize;
		}
	};
	return ret;
}

CDT* freeCDT(CDT* toFree) { 
	cdtIndex--;
	return &cdtPool[cdtIndex]; 
}

void initCDT() {
	int i=0;
	cdtPool = malloc(sizeof(CDT) * CDTSIZE);

	cdtIndex = 0;
	cdtSize = CDTSIZE;

	for (i=0; i<CDTSIZE; i++) {
    	cdtPool[i].time = (UInt64*) calloc(storageSize, sizeof(UInt64));
	    cdtPool[i].version = (UInt32*) calloc(storageSize, sizeof(UInt32));
		cdtPool[i].size = storageSize;

		assert(cdtPool[i].time && cdtPool[i].version);
		//fprintf(stderr,"cdtPool[%d].time = %p\n",i,cdtPool[i].time);
		//fprintf(stderr,"cdtPool[%d].version = %p\n",i,cdtPool[i].version);
	}
	cdtHead = allocCDT();
}

void deinitCDT() {
	int i=0;
	for (i=0; i<cdtSize; i++) {
		//fprintf(stderr,"deinit %d\n",i);
		//fprintf(stderr,"cdtPool[%d].time = %p\n",i,cdtPool[i].time);
		//fprintf(stderr,"cdtPool[%d].version = %p\n",i,cdtPool[i].version);
		free(cdtPool[i].time);
		free(cdtPool[i].version);
	}
}

void addControlDep(UInt cond) {
    MSG(2, "push ControlDep ts[%u]\n", cond);

#ifndef WORK_ONLY
    if(isCurrentLevelInstrumentable()) { cdtHead = allocCDT(); }

    if (isKremlinOn()) {
        TEntry* entry = getLTEntry(cond);
        fillCDT(cdtHead, entry);
    }
#endif
}

void removeControlDep() {
    MSG(2, "pop  ControlDep\n");
#ifndef WORK_ONLY
    if(isCurrentLevelInstrumentable()) { cdtHead = freeCDT(cdtHead); }
#endif
}


// prepare timestamp storage for return value
void addReturnValueLink(UInt dest) {
    if (!isKremlinOn())
        return;
    MSG(1, "prepare return storage ts[%u]\n", dest);
#ifndef WORK_ONLY
    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    funcHead->ret = getLTEntry(dest);
#endif
}

// write timestamp to the prepared storage
// TODO: implement
void logFuncReturn(UInt src) {
    if (!isKremlinOn())
        return;
    MSG(1, "write return value ts[%u]\n", src);

#ifndef WORK_ONLY
    TEntry* srcEntry = getLTEntry(src);

    // Assert there is a function context before the top.
    assert(FuncContextsSize(funcContexts) > 1);

    // Assert that its return value has been set.
    FuncContext** nextHead = FuncContextsLast(funcContexts) - 1;

    // Skip of the caller did not set up a return value location (i.e. lib functions).
    if(!(*nextHead)->ret) return;

    // Copy the return timestamp into the previous stack's return value.
    copyTEntry((*nextHead)->ret, srcEntry);
#endif
}

// TODO: implement
void logFuncReturnConst(void) {
    if (!isKremlinOn())
        return;
    MSG(1, "logFuncReturnConst\n");

#ifndef WORK_ONLY
    int i;
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    // Assert there is a function context before the top.
    assert(FuncContextsSize(funcContexts) > 1);

    FuncContext** nextHead = FuncContextsLast(funcContexts) - 1;

    // Skip of the caller did not set up a return value location (i.e. lib functions).
    if(!(*nextHead)->ret) return;

    TEntryAllocAtLeastLevel((*nextHead)->ret, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        int version = getVersion(i);
        UInt64 cdt = getCdt(i,version);

        // Copy the return timestamp into the previous stack's return value.
        updateTimestamp((*nextHead)->ret, i, version, cdt);
    }
#endif
}

void logBBVisit(UInt bb_id) {
    if (!isKremlinOn()) return;

#ifdef MANAGE_BB_INFO
    MSG(1, "logBBVisit(%u)\n", bb_id);
    __prevBB = __currentBB;
    __currentBB = bb_id;
#endif
}

void* logPhiNode1CD(UInt dest, UInt src, UInt cd) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "logPhiNode1CD ts[%u] = max(ts[%u], ts[%u])\n", dest, src, cd);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	TSArray* cd_tsa = getSrcTSArray(1);

	getLocalTimes(src, src_tsa);
	getLocalTimes(cd, cd_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		dest_tsa->times[i] = MAX(src_tsa->times[i],cd_tsa->times[i]);
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD = getLTEntry(cd);
    TEntry* entryDest = getLTEntry(dest);

    assert(entrySrc != NULL);
    assert(entryCD != NULL);
    assert(entryDest != NULL);
    
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    int i;
    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 ts_src = getTimestamp(entrySrc, i, version);
        UInt64 ts_cd = getTimestamp(entryCD, i, version);
        UInt64 max = (ts_src > ts_cd) ? ts_src : ts_cd;
        updateTimestamp(entryDest, i, version, max);
        MSG(2, "logPhiNode1CD level %u version %u \n", i, version);
        MSG(2, " src %u cd %u dest %u\n", src, cd, dest);
        MSG(2, " ts_src %u ts_cd %u max %u\n", ts_src, ts_cd, max);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "logPhiNode2CD ts[%u] = max(ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	TSArray* cd1_tsa = getSrcTSArray(1);
	TSArray* cd2_tsa = getSrcTSArray(2);

	getLocalTimes(src, src_tsa);
	getLocalTimes(cd1, cd1_tsa);
	getLocalTimes(cd2, cd2_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		Timestamp tmp_max_ts = MAX(src_tsa->times[i],cd1_tsa->times[i]);
		dest_tsa->times[i] = MAX(tmp_max_ts,cd2_tsa->times[i]);
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryDest = getLTEntry(dest);

    assert(entrySrc != NULL);
    assert(entryCD1 != NULL);
    assert(entryCD2 != NULL);
    assert(entryDest != NULL);
    
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    int i;
    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 ts_src = getTimestamp(entrySrc, i, version);
        UInt64 ts_cd1 = getTimestamp(entryCD1, i, version);
        UInt64 ts_cd2 = getTimestamp(entryCD2, i, version);
        UInt64 max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        UInt64 max2 = (max1 > ts_cd2) ? max1 : ts_cd2;

        updateTimestamp(entryDest, i, version, max2);

        MSG(2, "logPhiNode2CD level %u version %u \n", i, version);
        MSG(2, " src %u cd1 %u cd2 %u dest %u\n", src, cd1, cd2, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u max %u\n", ts_src, ts_cd1, ts_cd2, max2);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "logPhiNode3CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	TSArray* cd1_tsa = getSrcTSArray(1);
	TSArray* cd2_tsa = getSrcTSArray(2);
	TSArray* cd3_tsa = getSrcTSArray(3);

	getLocalTimes(src, src_tsa);
	getLocalTimes(cd1, cd1_tsa);
	getLocalTimes(cd2, cd2_tsa);
	getLocalTimes(cd3, cd3_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		Timestamp tmp_max_ts = MAX(src_tsa->times[i],cd1_tsa->times[i]);
		tmp_max_ts = MAX(tmp_max_ts,cd2_tsa->times[i]);
		dest_tsa->times[i] = MAX(tmp_max_ts,cd3_tsa->times[i]);
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryCD3 = getLTEntry(cd3);
    TEntry* entryDest = getLTEntry(dest);

    assert(entrySrc != NULL);
    assert(entryCD1 != NULL);
    assert(entryCD2 != NULL);
    assert(entryCD3 != NULL);
    assert(entryDest != NULL);
    
    int i = 0;
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 ts_src = getTimestamp(entrySrc, i, version);
        UInt64 ts_cd1 = getTimestamp(entryCD1, i, version);
        UInt64 ts_cd2 = getTimestamp(entryCD2, i, version);
        UInt64 ts_cd3 = getTimestamp(entryCD3, i, version);
        UInt64 max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        UInt64 max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        UInt64 max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        updateTimestamp(entryDest, i, version, max3);
        MSG(2, "logPhiNode3CD level %u version %u \n", i, version);
        MSG(2, " src %u cd1 %u cd2 %u cd3 %u dest %u\n", src, cd1, cd2, cd3, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u max %u\n", ts_src, ts_cd1, ts_cd2, ts_cd3, max3);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "logPhiNode4CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	TSArray* cd1_tsa = getSrcTSArray(1);
	TSArray* cd2_tsa = getSrcTSArray(2);
	TSArray* cd3_tsa = getSrcTSArray(3);
	TSArray* cd4_tsa = getSrcTSArray(3);

	getLocalTimes(src, src_tsa);
	getLocalTimes(cd1, cd1_tsa);
	getLocalTimes(cd2, cd2_tsa);
	getLocalTimes(cd3, cd3_tsa);
	getLocalTimes(cd4, cd4_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		Timestamp tmp_max_ts = MAX(src_tsa->times[i],cd1_tsa->times[i]);
		Timestamp tmp2_max_ts = MAX(cd2_tsa->times[i],cd3_tsa->times[i]);
		tmp_max_ts = MAX(tmp_max_ts,tmp2_max_ts);
		dest_tsa->times[i] = MAX(tmp_max_ts,cd4_tsa->times[i]);
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryCD3 = getLTEntry(cd3);
    TEntry* entryCD4 = getLTEntry(cd4);
    TEntry* entryDest = getLTEntry(dest);

    int i = 0;
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 ts_src = getTimestamp(entrySrc, i, version);
        UInt64 ts_cd1 = getTimestamp(entryCD1, i, version);
        UInt64 ts_cd2 = getTimestamp(entryCD2, i, version);
        UInt64 ts_cd3 = getTimestamp(entryCD3, i, version);
        UInt64 ts_cd4 = getTimestamp(entryCD4, i, version);
        UInt64 max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        UInt64 max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        UInt64 max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        UInt64 max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
        updateTimestamp(entryDest, i, version, max4);
        MSG(2, "logPhiNode4CD level %u version %u \n", i, version);
        MSG(2, " src %u cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", src, cd1, cd2, cd3, cd4, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", ts_src, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "log4CDToPhiNode ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, dest, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	TSArray* cd1_tsa = getSrcTSArray(1);
	TSArray* cd2_tsa = getSrcTSArray(2);
	TSArray* cd3_tsa = getSrcTSArray(3);
	TSArray* cd4_tsa = getSrcTSArray(3);

	getLocalTimes(dest, src_tsa);
	getLocalTimes(cd1, cd1_tsa);
	getLocalTimes(cd2, cd2_tsa);
	getLocalTimes(cd3, cd3_tsa);
	getLocalTimes(cd4, cd4_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		Timestamp tmp_max_ts = MAX(src_tsa->times[i],cd1_tsa->times[i]);
		Timestamp tmp2_max_ts = MAX(cd2_tsa->times[i],cd3_tsa->times[i]);
		tmp_max_ts = MAX(tmp_max_ts,tmp2_max_ts);
		dest_tsa->times[i] = MAX(tmp_max_ts,cd4_tsa->times[i]);
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryCD3 = getLTEntry(cd3);
    TEntry* entryCD4 = getLTEntry(cd4);
    TEntry* entryDest = getLTEntry(dest);

    int i = 0;
    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 ts_dest = getTimestamp(entryDest, i, version);
        UInt64 ts_cd1 = getTimestamp(entryCD1, i, version);
        UInt64 ts_cd2 = getTimestamp(entryCD2, i, version);
        UInt64 ts_cd3 = getTimestamp(entryCD3, i, version);
        UInt64 ts_cd4 = getTimestamp(entryCD4, i, version);
        UInt64 max1 = (ts_dest > ts_cd1) ? ts_dest : ts_cd1;
        UInt64 max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        UInt64 max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        UInt64 max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
        updateTimestamp(entryDest, i, version, max4);
        MSG(2, "log4CDToPhiNode4CD level %u version %u \n", i, version);
        MSG(2, " cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", cd1, cd2, cd3, cd4, dest);
        MSG(2, " ts_dest %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", ts_dest, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
    }

    return entryDest;
	*/
#else
    return NULL;
#endif
}

#define MAX_ENTRY 10

void logPhiNodeAddCondition(UInt dest, UInt src) {
    if (!isKremlinOn()) return;

    MSG(1, "logPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest, src, dest);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

	TSArray* src_tsa = getSrcTSArray(0);
	TSArray* orig_dest_tsa = getSrcTSArray(1);

	getLocalTimes(src, src_tsa);
	getLocalTimes(dest, orig_dest_tsa);

	TSArray* dest_tsa = getDestTSArray();

    int i;
    for (i = 0; i < numActiveLevels; ++i) {
		dest_tsa->times[i] = MAX(src_tsa->times[i],orig_dest_tsa->times[i]);
	}

	updateLocalTimes(dest,dest_tsa);

    return dest_tsa;

	/*
    TEntry* entry0 = getLTEntry(src);
    TEntry* entry1 = getLTEntry(dest);
    TEntry* entryDest = entry1;
    
    assert(entry0 != NULL);
    assert(entry1 != NULL);
    assert(entryDest != NULL);

    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());
    int i = 0;

    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 ts0 = getTimestamp(entry0, i, version);
        UInt64 ts1 = getTimestamp(entry1, i, version);
        UInt64 value = (ts0 > ts1) ? ts0 : ts1;
        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
        MSG(2, "logPhiAddCond level %u version %u \n", i, version);
        MSG(2, " src %u dest %u\n", src, dest);
        MSG(2, " ts0 %u ts1 %u value %u\n", ts0, ts1, value);
    }
	*/
#endif
}

// use estimated cost for a callee function we cannot instrument
// TODO: implement new shadow mem interface
void* logLibraryCall(UInt cost, UInt dest, UInt num_in, ...) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "logLibraryCall to ts[%u] with cost %u\n", dest, cost);
    addWork(cost);

#ifndef WORK_ONLY
    TEntry* entrySrc[MAX_ENTRY];
    TEntry* entryDest = getLTEntry(dest);

    va_list ap;
    va_start(ap, num_in);

    int i;
    for (i = 0; i < num_in; i++) {
        UInt src = va_arg(ap, UInt);
        entrySrc[i] = getLTEntry(src);
        assert(entrySrc[i] != NULL);
    }   
    va_end(ap);

    int minLevel = __kremlin_min_level;
    int maxLevel = MIN(__kremlin_max_profiled_level, getCurrentLevel());

    TEntryAllocAtLeastLevel(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 max = 0;
        
        int j;
        for (j = 0; j < num_in; j++) {
            UInt64 ts = getTimestamp(entrySrc[j], i, version);
            if (ts > max)
                max = ts;
        }   
        
		UInt64 value = max + cost;

        updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
    }
    return entryDest;
#else
    return NULL;
#endif
    
}

// this function is the same as logAssignmentConst but helps to quickly
// identify induction variables in the source code
void* logInductionVar(UInt dest) {
    if (!isKremlinOn()) return NULL;

    return logAssignmentConst(dest);
}

UInt isCpp = FALSE;
UInt hasInitialized = 0;

int kremlinInit() {
    if(hasInitialized++) {
        MSG(0, "kremlinInit skipped\n");
        return FALSE;
    }
    MSG(0, "kremlinInit running\n");

    kremlinOn = TRUE;

	if(__kremlin_debug) { fprintf(stderr,"[kremlin] debugging enabled at level %d\n", __kremlin_debug_level); }

#ifdef USE_UREGION
    initializeUdr();
#else
	cregionInit();
#endif
    levelNum = -1;

    InvokeRecordsCreate(&invokeRecords);

    storageSize = (__kremlin_max_profiled_level - __kremlin_min_level)+1;
    MSG(0, "minLevel = %d maxLevel (profiled) = %d storageSize = %d\n", 
        __kremlin_min_level, __kremlin_max_profiled_level, storageSize);

    // Allocate a memory allocator.
    MemMapAllocatorCreate(&memPool, ALLOCATOR_SIZE);

    // Emulates the call stack.
    FuncContextsCreate(&funcContexts);
    initDataStructure(storageSize);

	// TODO: versions shouldn't be statically allocated to
	// DS_ALLOC_SIZE... this should be dynamic
	// Can we use storageSize instead?
    versions = (int*) calloc(sizeof(int), DS_ALLOC_SIZE);
    assert(versions);

    regionInfo = (Region*) calloc(sizeof(Region), storageSize);
    assert(regionInfo);

    // Allocate a deque to hold timestamps of args.
    deque_create(&argTimestamps, NULL, NULL);

    // Allocate the hash map to store dynamic region id counts.
    hash_map_sid_did_create(&sidToDid, sidHash, sidCompare, NULL, NULL);

    GTableCreate(&gTable);

    allocDummyTEntry();

	initCDT();
    initStartFuncContext();
    
	turnOnProfiler();
    return TRUE;
}

static TEntry* mainReturn;
void deinitStartFuncContext()
{
	popFuncContext();

    assert(FuncContextsEmpty(funcContexts));
    freeTEntry(mainReturn);
    mainReturn = NULL;
}

void initStartFuncContext()
{
    assert(FuncContextsEmpty(funcContexts));

    prepareCall(0, 0);
	pushFuncContext();
    (*FuncContextsLast(funcContexts))->ret = mainReturn = allocTEntry();
}


int kremlinDeinit() {
    if(--hasInitialized) {
        MSG(0, "kremlinDeinit skipped\n");
        return FALSE;
    }
    MSG(0, "kremlinDeinit running\n");

	turnOffProfiler();

    deinitStartFuncContext();

#ifdef USE_UREGION
    finalizeUdr();
#else
	//cregionFinish("kremlin.bin");
	cregionFinish(__kremlin_output_filename);
#endif
    freeDummyTEntry();

    finalizeDataStructure();

    // Deallocate the arg timestamp deque.
    deque_delete(&argTimestamps);

    // Deallocate the static id to dynamic id map.
    hash_map_sid_did_delete(&sidToDid);

    free(regionInfo);
    regionInfo = NULL;

    free(versions);
    versions = NULL;


    cdtHead = NULL;

    // Deallocate the memory allocator.
    MemMapAllocatorDelete(&memPool);

    // Emulates the call stack.
    FuncContextsDelete(&funcContexts);
	deinitCDT();

    GTableDelete(&gTable);

    kremlinOn = FALSE;

    dumpTableMemAlloc();

    return TRUE;
}

void initProfiler() {
    kremlinInit();
}

void cppEntry() {
    isCpp = TRUE;
    kremlinInit();
}

void cppExit() {
    kremlinDeinit();
}

void deinitProfiler() {
    kremlinDeinit();
}

UInt64 sidHash(UInt64 sid) {
    return sid;
}
void logLoopIteration() {}
int sidCompare(UInt64 s1, UInt64 s2) {
    return s1 == s2;
}

void printProfileData() {}

int parseOptionInt(char* option_str) {
	char *dbg_level_str = strtok(option_str,"= ");
	dbg_level_str = strtok(NULL,"= ");

	if(dbg_level_str) {
		return atoi(dbg_level_str);
	}
	else {
		fprintf(stderr,"ERROR: Couldn't parse int from option (%s)\n",option_str);
	}
}

void createOutputFilename() {
	__kremlin_output_filename[0] = '\0'; // "clear" the old name

	strcat(__kremlin_output_filename,"kremlin-L");
	char level_str[5];
	//sprintf(level_str,"%d",__kremlin_level_to_log);
	sprintf(level_str,"%d",__kremlin_min_level);
	strcat(__kremlin_output_filename,level_str);

	if(__kremlin_max_level != (__kremlin_min_level)) {
		strcat(__kremlin_output_filename,"_");
		level_str[0] - '\0';
		sprintf(level_str,"%d",__kremlin_max_level);
		strcat(__kremlin_output_filename,level_str);
	}
	
	strcat(__kremlin_output_filename,".bin");
}

void parseKremlinOptions(int argc, char* argv[], int* num_args, char*** real_args) {
	int num_true_args = 0;
	char** true_args = malloc(argc*sizeof(char*));

	int i;
	for(i = 0; i < argc; ++i) {
		//fprintf(stderr,"checking %s\n",argv[i]);
		char *str_start;

		str_start = strstr(argv[i],"kremlin-debug");

		if(str_start) {
			__kremlin_debug= 1;
			__kremlin_debug_level = parseOptionInt(argv[i]);

			continue;
		}

		str_start = strstr(argv[i],"kremlin-ltl");
		if(str_start) {
			__kremlin_level_to_log = parseOptionInt(argv[i]);
			__kremlin_min_level = __kremlin_level_to_log;
			__kremlin_max_level = __kremlin_level_to_log;
			__kremlin_max_profiled_level = __kremlin_max_level + 1;

			continue;
		}

		str_start = strstr(argv[i],"kremlin-min-level");
		if(str_start) {
			__kremlin_min_level = parseOptionInt(argv[i]);
			continue;
		}

		str_start = strstr(argv[i],"kremlin-max-level");
		if(str_start) {
			__kremlin_max_level = parseOptionInt(argv[i]);
			__kremlin_max_profiled_level = __kremlin_max_level + 1;
			continue;
		}
		else {
			true_args[num_true_args] = strdup(argv[i]);
			num_true_args++;
		}
	}

	createOutputFilename();

	true_args = realloc(true_args,num_true_args*sizeof(char*));

	*num_args = num_true_args;
	*real_args = true_args;
}

// look for any kremlin specific inputs to the program
int main(int argc, char* argv[]) {
	int num_args = 0;;
	char** real_args;

	__kremlin_output_filename = malloc(20*sizeof(char));
	strcat(__kremlin_output_filename,"kremlin.bin");

	parseKremlinOptions(argc,argv,&num_args,&real_args);

	if(__kremlin_level_to_log == -1) {
    	fprintf(stderr, "[kremlin] min level = %d, max level = %d\n", __kremlin_min_level, __kremlin_max_level);
	}
	else {
    	fprintf(stderr, "[kremlin] logging only level %d\n", __kremlin_level_to_log);
	}

	fprintf(stderr,"[kremlin] writing data to: %s\n",__kremlin_output_filename);

	__main(argc,argv);
}

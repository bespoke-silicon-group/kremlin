#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "kremlin.h"
#include "log.h"
#include "debug.h"
#include "GTable.h"
#include "kremlin_deque.h"
#include "cregion.h"

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)
#define DS_ALLOC_SIZE   100     // used for static data structures
#define MAX_SRC_TSA_VAL	6

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))



UInt64*     getDynamicRegionId(UInt64 sid);
void        incDynamicRegionId(UInt64 sid);
UInt64      sidHash(UInt64 sid);
int         sidCompare(UInt64 s1, UInt64 s2);

int                 kremlinOn = 0;
int                 levelNum = 0;
UInt32				storageSize = 0;
int					numActiveLevels = 0; // number of levels currently being logged

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
extern GTable*		gTable; 		// dhjeon: remove when decoupling of gtable is done

#ifdef MANAGE_BB_INFO
UInt    __prevBB;
UInt    __currentBB;
#endif


UInt64 _regionFuncCnt;
UInt64 _setupTableCnt;
int _requireSetupTable;

// Dummy entries required for initialization
// DJ Question: why dummy is required?
static TEntry* dummyEntry = NULL;
static void allocDummyTEntry() { dummyEntry = allocTEntry(); }
static TEntry* getDummyTEntry() { return dummyEntry; }
static void freeDummyTEntry() {
   //freeTEntry(dummyEntry);
   free(dummyEntry); // dummy will always have NULL for time/version
   dummyEntry = NULL;
}

// precondition: inLevel >= getMinReportLevel() && inLevel < maxRegionSize
inline Timestamp getTimestamp(TEntry* entry, Level inLevel, Version version) {
	Level level = inLevel - getMinReportLevel();
    Timestamp ret = (level >= 0 && level < entry->timeArrayLength && entry->version[level] == version) ?
				entry->time[level] : 0;
	return ret;
}


// precondition: entry != NULL
inline void updateTimestamp(TEntry* entry, Level inLevel, Version version, Timestamp timestamp) {
	Level level = inLevel - getMinReportLevel();
	TEntryAllocAtLeastLevel(entry, level);
	entry->version[level] = version;
	entry->time[level] = timestamp;
}

// what are lowest and highest levels to instrument now?
inline Level getStartLevel() {
	return getMinReportLevel();
}

inline Level getEndLevel() {
    return MIN(getMaxProfileLevel(), getCurrentLevel());
}


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



void prepareCall(UInt64 callSiteId, UInt64 calledRegionId) {
    if(!isKremlinOn()) { return; }

    // Clear off any argument timestamps that have been left here before the
    // call. These are left on the deque because library calls never take
    // theirs off. 
    deque_clear(argTimestamps);
	lastCallSiteId = callSiteId;
}

// TODO: need to think how to pass args without TEntry
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

UInt getVersion(int level) {
	assert(level >= 0 && level < DS_ALLOC_SIZE);
	return versions[level];
}

/**
 * Returns timestamp of control dep at specified level with specified version.
 * @param level 	Level at which to look for control dep.
 * @param version	Version we are looking for.
 * @return			Timestamp of control dep.
 */
// preconditions: cdtHead != NULL && level >= 0
Timestamp getCdt(int level, UInt32 version) {
    //assert(cdtHead != NULL);
    return (cdtHead->version[level - getMinReportLevel()] == version) ?
        cdtHead->time[level - getMinReportLevel()] : 0;
}

Timestamp getCurrentCdt(int level) {
	return getCdt(level, getVersion(level));
}

/**
 * Sets the control dep at specified level to specified (version,time) pair
 * @param level		Level to set.
 * @param version	Version number to set.
 * @param time		Timestamp to set control dep to.
 */
void setCdt(int level, UInt32 version, Timestamp time) {
    assert(level >= getMinReportLevel());
    cdtHead->time[level - getMinReportLevel()] = time;
    cdtHead->version[level - getMinReportLevel()] = version;
}

/**
 * Copies data from table entry over to cdt.
 * @param cdt		Pointer to CDT to copy data to.
 * @param entry		Pointer to TEntry containing data to be copied.
 */
void fillCDT(CDT* cdt, TEntry* entry) {
	// entry may not have data for all logged levels so we have to check timeArrayLength
    int i;
    for (i = getMinReportLevel(); i <= getMaxProfileLevel(); ++i) {
        int level = i - getMinReportLevel();

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
	int true_level = level - getMinReportLevel();
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

    FuncContext* funcHead = *FuncContextsLast(funcContexts);
	UInt64 callSiteId = (funcHead == NULL) ? 0x0 : funcHead->callSiteId;
	cregionPutContext(regionId, callSiteId);

	if(isCurrentLevelInstrumentable()) numActiveLevels++;

	// If we exceed the maximum depth, we act like this region doesn't exist
	if(curr_level > getMaxProfileLevel()) { return; }


    incDynamicRegionId(regionId);
    versions[curr_level]++;

    if (regionType < RegionLoopBody)
        MSG(0, "[+++] region [%u, %d, %llu:%llu] start: %llu\n",
            regionType, curr_level, regionId, *getDynamicRegionId(regionId), timestamp);

	// set initial values for newly entered region (but make sure we are only
	// doing this for logged levels)
	if(curr_level >= getMinReportLevel()) {
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
		cregionRemoveContext(NULL);
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
	if (curr_level > getMinReportLevel()) {
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

	cregionRemoveContext(&field);
        
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

    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; ++i) {
		// calculate availability time
        Timestamp cdt = getCurrentCdt(i);
        Timestamp ts0 = regGetTimestamp(src0, i);
        Timestamp ts1 = regGetTimestamp(src1, i);
        Timestamp greater0 = (ts0 > ts1) ? ts0 : ts1;
        Timestamp greater1 = (cdt > greater0) ? cdt : greater0;
        Timestamp value = greater1 + opCost;
		regSetTimestamp(value, dest, i);
        updateCP(value, i);
		
        MSG(2, "binOp[%u] level %u version %u \n", opCost, i, getVersion(i));
        MSG(2, " src0 %u src1 %u dest %u\n", src0, src1, dest);
        MSG(2, " ts0 %u ts1 %u cdt %u value %u\n", ts0, ts1, cdt, value);
    }

//  return entryDest;
	return NULL;
	
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

    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();


    int i;
    for (i = minLevel; i <= maxLevel; i++) {
        Timestamp cdt = getCurrentCdt(i);
        Timestamp ts0 = regGetTimestamp(src, i);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + opCost;

		// update time and critical path info
		regSetTimestamp(value, dest, i);
        updateCP(value, i);

        MSG(2, "binOpConst[%u] level %u version %u \n", opCost, i, getVersion(i));
        MSG(2, " src %u dest %u\n", src, dest);
        MSG(2, " ts0 %u cdt %u value %u\n", ts0, cdt, value);
    }

//    return entryDest;
    return NULL;
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
    
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
        Timestamp cdt = getCurrentCdt(i);
		regSetTimestamp(cdt, dest, i);
        updateCP(cdt, i);
    }

    //return entryDest;
    return NULL;
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

    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

#ifdef EXTRA_STATS
    TEntryAllocAtLeastLevel(entry0, maxLevel);
#endif

    Level i;
    for (i = minLevel; i <= maxLevel; i++) {
        regionInfo[getLevelOffset(i)].loadCnt++;
        Timestamp cdt = getCurrentCdt(i);
		Timestamp ts0 = memGetTimestamp(src_addr, i);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + LOAD_COST;

#ifdef EXTRA_STATS
        updateReadMemoryAccess(entry0, i, getVersion(i), value);
#endif

        regSetTimestamp(value, dest, i);
        updateCP(value, i);
    }

    //return entryDest;
    return NULL;
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

    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;

    for (i = minLevel; i <= maxLevel; i++) {
        Timestamp cdt = getCurrentCdt(i);
		Timestamp ts_src_addr = memGetTimestamp(src1, i);
		Timestamp ts_src1 = regGetTimestamp(src1, i);

        Timestamp max1 = (ts_src_addr > cdt) ? ts_src_addr : cdt;
        Timestamp max2 = (max1 > ts_src1) ? max1 : ts_src1;
		Timestamp value = max2 + LOAD_COST;

        //updateTimestamp(entryDest, i, version, value);
        regSetTimestamp(value, dest, i);
        updateCP(value, i);

        MSG(2, "logLoadInst1Src level %u version %u \n", i, getVersion(i));
        MSG(2, " src_addr 0x%x src1 %u dest %u\n", src_addr, src1, dest);
        MSG(2, " cdt %u ts_src_addr %u ts_src1 %u max %u\n", cdt, ts_src_addr, ts_src1, max2);
    }

    //return entryDest;
    return NULL;
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

    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    //fprintf(stderr, "\n\nstore ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);
    int i;
    for (i = minLevel; i <= maxLevel; i++) {
        regionInfo[getLevelOffset(i)].storeCnt++;
        Timestamp cdt = getCurrentCdt(i);
		Timestamp ts0 = regGetTimestamp(src, i);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + STORE_COST;
#ifdef EXTRA_STATS
        updateWriteMemoryAccess(entryDest, i, getVersion(i), value);
#endif
		memSetTimestamp(value, dest_addr, i);
        updateCP(value, i);
    }

    //return entryDest;
    return NULL;
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

#ifdef EXTRA_STATS
    TEntry* entryLine = getGTEntryCacheLine(dest_addr);
#endif
    
    //fprintf(stderr, "\nstoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; ++i) {
        Timestamp cdt = getCurrentCdt(i);
        Timestamp value = cdt + STORE_COST;
#ifdef EXTRA_STATS
        //updateWriteMemoryAccess(entryDest, i, version, value);
        //updateWriteMemoryLineAccess(entryLine, i, version, value);
#endif
		memSetTimestamp(value, dest_addr, i);
        updateCP(value, i);
    }

   // return entryDest;

    return NULL;
#else
    return NULL;
#endif
}

// FIXME: support 64 bit address
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
    for(a = addr; a < addr + mem_size; a++) {
        GTableDeleteTEntry(gTable, a);
	}

    freeMEntry(addr);

	addWork(FREE_COST);

	// make sure CP is at least the time needed to complete the free
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

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
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    // Assert there is a function context before the top.
    assert(FuncContextsSize(funcContexts) > 1);

    FuncContext** nextHead = FuncContextsLast(funcContexts) - 1;

    // Skip of the caller did not set up a return value location (i.e. lib functions).
    if(!(*nextHead)->ret) return;

    TEntryAllocAtLeastLevel((*nextHead)->ret, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        int version = getVersion(i);
        UInt64 cdt = getCurrentCdt(i);

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
    
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts_src = regGetTimestamp(src, i);
		Timestamp ts_cd = regGetTimestamp(cd, i);
        Timestamp max = (ts_src > ts_cd) ? ts_src : ts_cd;
        //updateTimestamp(entryDest, i, version, max);
		regSetTimestamp(max, dest, i);
        MSG(2, "logPhiNode1CD level %u version %u \n", i, getVersion(i));
        MSG(2, " src %u cd %u dest %u\n", src, cd, dest);
        MSG(2, " ts_src %u ts_cd %u max %u\n", ts_src, ts_cd, max);
    }

    //return entryDest;
    return NULL;
#else
    return NULL;
#endif
}

void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "logPhiNode2CD ts[%u] = max(ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;
    
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts_src = regGetTimestamp(src, i);
		Timestamp ts_cd1 = regGetTimestamp(cd1, i);
		Timestamp ts_cd2 = regGetTimestamp(cd2, i);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;

		regSetTimestamp(max2, dest, i);
        //updateTimestamp(entryDest, i, version, max2);

        MSG(2, "logPhiNode2CD level %u version %u \n", i, getVersion(i));
        MSG(2, " src %u cd1 %u cd2 %u dest %u\n", src, cd1, cd2, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u max %u\n", ts_src, ts_cd1, ts_cd2, max2);
    }

    //return entryDest;
    return NULL;
#else
    return NULL;
#endif
}

void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "logPhiNode3CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts_src = regGetTimestamp(src, i);
		Timestamp ts_cd1 = regGetTimestamp(cd1, i);
		Timestamp ts_cd2 = regGetTimestamp(cd2, i);
		Timestamp ts_cd3 = regGetTimestamp(cd3, i);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;

		regSetTimestamp(max3, dest, i);

        MSG(2, "logPhiNode3CD level %u version %u \n", i, getVersion(i));
        MSG(2, " src %u cd1 %u cd2 %u cd3 %u dest %u\n", src, cd1, cd2, cd3, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u max %u\n", ts_src, ts_cd1, ts_cd2, ts_cd3, max3);
    }

    //return entryDest;
    return NULL;
#else
    return NULL;
#endif
}

void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "logPhiNode4CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts_src = regGetTimestamp(src, i);
		Timestamp ts_cd1 = regGetTimestamp(cd1, i);
		Timestamp ts_cd2 = regGetTimestamp(cd2, i);
		Timestamp ts_cd3 = regGetTimestamp(cd3, i);
		Timestamp ts_cd4 = regGetTimestamp(cd4, i);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Timestamp max4 = (max3 > ts_cd4) ? max3 : ts_cd4;

		regSetTimestamp(max4, dest, i);

        MSG(2, "logPhiNode4CD level %u version %u \n", i, getVersion(i));
        MSG(2, " src %u cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", src, cd1, cd2, cd3, cd4, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", ts_src, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
    }

    //return entryDest;
    return NULL;
#else
    return NULL;
#endif
}

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "log4CDToPhiNode ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, dest, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;
	
    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
        Timestamp ts_dest = regGetTimestamp(dest, i);
		Timestamp ts_cd1 = regGetTimestamp(cd1, i);
		Timestamp ts_cd2 = regGetTimestamp(cd2, i);
		Timestamp ts_cd3 = regGetTimestamp(cd3, i);
		Timestamp ts_cd4 = regGetTimestamp(cd4, i);
        Timestamp max1 = (ts_dest > ts_cd1) ? ts_dest : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Timestamp max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
		regSetTimestamp(max4, dest, i);

        MSG(2, "log4CDToPhiNode4CD level %u version %u \n", i, getVersion(i));
        MSG(2, " cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", cd1, cd2, cd3, cd4, dest);
        MSG(2, " ts_dest %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", ts_dest, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
    }
   // return entryDest;
    return NULL;
#else
    return NULL;
#endif
}

#define MAX_ENTRY 10

void* logPhiNodeAddCondition(UInt dest, UInt src) {
    if (!isKremlinOn()) return;

    MSG(1, "logPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest, src, dest);

#ifndef WORK_ONLY
	if(numActiveLevels == 0) return NULL;

    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();
    int i = 0;

    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts0 = regGetTimestamp(src, i);
		Timestamp ts1 = regGetTimestamp(dest, i);
        Timestamp value = (ts0 > ts1) ? ts0 : ts1;
		regSetTimestamp(value, dest, i);
        updateCP(value, i);
        MSG(2, "logPhiAddCond level %u version %u \n", i, getVersion(i));
        MSG(2, " src %u dest %u\n", src, dest);
        MSG(2, " ts0 %u ts1 %u value %u\n", ts0, ts1, value);
    }
	
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

    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

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

UInt isCpp = FALSE;
UInt hasInitialized = 0;

int kremlinInit() {
    if(hasInitialized++) {
        MSG(0, "kremlinInit skipped\n");
        return FALSE;
    }
    MSG(0, "kremlinInit running\n");

    kremlinOn = TRUE;

	if(getKremlinDebugFlag()) { 
		fprintf(stderr,"[kremlin] debugging enabled at level %d\n", getKremlinDebugLevel()); 
	}

	cregionInit();
    levelNum = -1;

    InvokeRecordsCreate(&invokeRecords);

    storageSize = (getMaxProfileLevel() - getMinReportLevel())+1;
    MSG(0, "minLevel = %d maxLevel (profiled) = %d storageSize = %d\n", 
        getMinReportLevel(), getMaxProfileLevel(), storageSize);

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

	memShadowInit();
    //GTableCreate(&gTable); // TODO: abstract this for new shadow mem imp

    allocDummyTEntry(); // TODO: abstract this for new shadow mem imp

	initCDT();
    initStartFuncContext();
    
	turnOnProfiler();
    return TRUE;
}




int kremlinDeinit() {
    if(--hasInitialized) {
        MSG(0, "kremlinDeinit skipped\n");
        return FALSE;
    }
    MSG(0, "kremlinDeinit running\n");

	turnOffProfiler();

    deinitStartFuncContext();

	//cregionFinish("kremlin.bin");
	cregionFinish(argGetOutputFileName());
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

    GTableDelete(&gTable); // TODO: abstract this for new shadow mem imp
    kremlinOn = FALSE;
    dumpTableMemAlloc(); // TODO: abstract this for new shadow mem imp

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


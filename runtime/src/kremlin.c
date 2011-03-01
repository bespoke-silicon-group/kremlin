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
#include "deque.h"
#include "hash_map.h"
#include "cregion.h"
#include "Vector.h"

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024)

#define _MAX_REGION_LEVEL   100     // used for static data structures

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))

UInt64*     getDynamicRegionId(UInt64 sid);
void        incDynamicRegionId(UInt64 sid);
UInt64      sidHash(UInt64 sid);
int         sidCompare(UInt64 s1, UInt64 s2);

typedef struct _CDT_T {
    UInt64* time;
    UInt32* version;
} CDT;

typedef struct _FuncContext {
    LTable* table;
    TEntry* ret;
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
#define getLevelNum()      (levelNum)
#define getCurrentLevel()  (levelNum-1)
#define isCurrentLevelInstrumentable() (((levelNum-1) >= MIN_REGION_LEVEL) && ((levelNum-1) <= MAX_REGION_LEVEL))

void initStartFuncContext();
HASH_MAP_DEFINE_PROTOTYPES(sid_did, UInt64, UInt64);
HASH_MAP_DEFINE_FUNCTIONS(sid_did, UInt64, UInt64);

// A vector used to represent the call stack.
VECTOR_DEFINE_PROTOTYPES(FuncContexts, FuncContext*);
VECTOR_DEFINE_FUNCTIONS(FuncContexts, FuncContext*, VECTOR_COPY, VECTOR_NO_DELETE);

// A vector used to record invoked calls.
VECTOR_DEFINE_PROTOTYPES(InvokeRecords, InvokeRecord);
VECTOR_DEFINE_FUNCTIONS(InvokeRecords, InvokeRecord, VECTOR_COPY, VECTOR_NO_DELETE);

#if 1
const int               _maxRegionToLog = MAX_REGION_LEVEL;
const int               _minRegionToLog = MIN_REGION_LEVEL;
#endif

int                 kremlinOn = 0;
int                 levelNum = 0;
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

#ifdef MANAGE_BB_INFO
UInt    __prevBB;
UInt    __currentBB;
#endif


UInt64 _regionFuncCnt;
UInt64 _setupTableCnt;
int _requireSetupTable;

/*
 * start profiling
 *
 * push the root region (id == 0, type == loop)
 * loop type seems weird, but using functino type as the root region
 * causes several problems regarding local table
 *
 * when kremlinOn == 0,
 * most instrumentation functions do nothing.
 */ 
void turnOnProfiler() {
	fprintf(stderr, "WARNING: turnOn/OffProfiler must be called at the same levels or the profiler will break.\n");
	fprintf(stderr, "turnOnProfiler...");
    kremlinOn = 1;
    logRegionEntry(0, 1);
	fprintf(stderr, "done\n");
}

/*
 * end profiling
 *
 * pop the root region pushed in turnOnProfiler()
 */
void turnOffProfiler() {
    logRegionExit(0, 1);
    kremlinOn = 0;
	fprintf(stderr, "turnOffProfiler\n");
}

void pauseProfiler() {
	kremlinOn = 0;
	fprintf(stderr, "pauseProfiler\n");
}

void resumeProfiler() {
	kremlinOn = 1;
	fprintf(stderr, "resumeProfiler\n");
}
int _maxRegionNum = 0;


TEntry* dummyEntry = NULL;
void allocDummyTEntry() {
    dummyEntry = (TEntry*) allocTEntry(getTEntrySize());
}

TEntry* getDummyTEntry() {
    return dummyEntry;
}

void freeDummyTEntry() {
    freeTEntry(dummyEntry);
    dummyEntry = NULL;
}

void prepareCall(UInt64 callSiteId, UInt64 calledRegionId) {
    if(!isKremlinOn())
        return;

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
    MSG(1, "getArgInfo to ts[%u]\n", dest);

#ifndef WORK_ONLY
    TEntry* destEntry = getLTEntry(dest);
    TEntry* srcEntry = deque_pop_front(argTimestamps);
    assert(destEntry);
    assert(srcEntry);
    copyTEntry(destEntry, srcEntry);
#endif
}


void setupLocalTable(UInt maxVregNum) {
    if(!isKremlinOn())
        return;

    MSG(1, "setupLocalTable size %u\n", maxVregNum);

#ifndef WORK_ONLY
    assert(_requireSetupTable == 1);

    LTable* table = allocLocalTable(maxVregNum);
    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    assert(funcHead->table == NULL);
    assert(table != NULL);

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

void decrementRegionLevel() {
    levelNum--;
}

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

// preconditions: cdtHead != NULL && level >= 0
UInt64 getCdt(int level, UInt32 version) {
    //assert(cdtHead != NULL);
    //assert(level >= 0);
    return (cdtHead->version[level - MIN_REGION_LEVEL] == version) ?
        cdtHead->time[level - MIN_REGION_LEVEL] : 0;
}

void setCdt(int level, UInt32 version, UInt64 time) {
    assert(level >= MIN_REGION_LEVEL);
    cdtHead->time[level - MIN_REGION_LEVEL] = time;
    cdtHead->version[level - MIN_REGION_LEVEL] = version;
}

void fillCDT(CDT* cdt, TEntry* entry) {
    int i;
    for (i = MIN_REGION_LEVEL; i < MAX_REGION_LEVEL; i++) {
        cdt->time[i - MIN_REGION_LEVEL] = entry->time[i - MIN_REGION_LEVEL];
        cdt->version[i - MIN_REGION_LEVEL] = entry->version[i - MIN_REGION_LEVEL];
    }
}

void pushFuncContext() {

    FuncContext* toAdd = (FuncContext*) malloc(sizeof(FuncContext));
    assert(toAdd);
    FuncContextsPushRef(funcContexts, &toAdd);
    toAdd->table = NULL;
	toAdd->callSiteId = lastCallSiteId;

#ifdef MANAGE_BB_INFO
    toAdd->retBB = __currentBB;
    toAdd->retPrevBB = __prevBB;
    MSG(1, "[push] current = %u last = %u\n", __currentBB, __prevBB);
#endif
	//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);

}

void dumpCdt(CDT* cdt) {
    int i;
    fprintf(stderr, "cdtHead@%p\n", cdt);
    for (i = 0; i < 5; i++) {
        fprintf(stderr, "\t%llu", cdt->time[i]);
    }
    fprintf(stderr, "\n");
}

void dumpTEntry(TEntry* entry, int size) {
    int i;
    fprintf(stderr, "entry@%p\n", entry);
    for (i = 0; i < size; i++) {
        fprintf(stderr, "\t%llu", entry->time[i]);
    }
    fprintf(stderr, "\n");
}

UInt64 updateCP(UInt64 value, int level) {
	regionInfo[level].cp = (value > regionInfo[level].cp) ? value : regionInfo[level].cp;
	return regionInfo[level].cp;

}

#define addWork(work) timestamp += work;

void addLoad() {
    loadCnt++;
}

void addStore() {
    storeCnt++;
}

void popFuncContext() {
    FuncContext* ret = FuncContextsPopVal(funcContexts);
    assert(ret);
    //assert(ret->table != NULL);
    // restore currentBB and prevBB
#ifdef MANAGE_BB_INFO
    __currentBB = ret->retBB;
    __prevBB = ret->retPrevBB;
#endif

    assert(_regionFuncCnt == _setupTableCnt);
    assert(_requireSetupTable == 0);
    //assert(ret->table != NULL);

    if (ret->table != NULL)
        freeLocalTable(ret->table);

    free(ret);  
}

UInt getVersion(int level) {
    assert(level >= 0 && level < _MAX_REGION_LEVEL);
    return versions[level];
}

// precondition: inLevel >= MIN_REGION_LEVEL && inLevel < maxRegionSize
inline UInt64 getTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
    int level = inLevel - MIN_REGION_LEVEL;

    UInt64 ret = (entry->version[level] == version) ?
                    entry->time[level] : 0;
    return ret;
}


// precondition: entry != NULL
inline void updateTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int level = inLevel - MIN_REGION_LEVEL;
    entry->version[level] = version;
    entry->time[level] = timestamp;
}

#ifdef EXTRA_STATS
UInt64 getReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
    int level = inLevel - MIN_REGION_LEVEL;
    assert(entry != NULL);
    assert(level >= 0 && level < getTEntrySize());
    UInt64 ret = (entry->readVersion[level] == version) ?
                    entry->readTime[level] : 0;
    return ret;
}

void updateReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int level = inLevel - MIN_REGION_LEVEL;
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
    currentRecord->stackHeight = FuncContextsSize(funcContexts);
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

    if(!InvokeRecordsEmpty(invokeRecords) && InvokeRecordsLast(invokeRecords)->id == id) {
        InvokeRecord* currentRecord = InvokeRecordsLast(invokeRecords);
        MSG(1, "invokeThrew(%u) - Popping to %d\n", currentRecord->id, currentRecord->stackHeight);

        while(getCurrentLevel() > currentRecord->stackHeight)
        {
            UInt64 lastLevel = getCurrentLevel();
            logRegionExit(regionInfo[getCurrentLevel()].regionId, 0);
            assert(getCurrentLevel() < lastLevel);
            assert(getCurrentLevel() >= 0);
        }
        InvokeRecordsPop(invokeRecords);
    }
    else
        MSG(1, "invokeThrew(%u) ignored\n", id);
}

void logRegionEntry(UInt64 regionId, UInt regionType) {
    if (!isKremlinOn()) {
        return;
    }

    if (regionType == 0)
        _regionFuncCnt++;

    if(regionType == RegionFunc)
    {
		/*
		if(getCurrentLevel() >= MAX_REGION_LEVEL-1) {
			fprintf(stderr,"entering region too deep to log. level = %d, lastCallSiteId = %llu\n",getCurrentLevel(),lastCallSiteId);
		}
		*/
        pushFuncContext();
        _requireSetupTable = 1;
    }

    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    incrementRegionLevel();

    int level = getCurrentLevel();

	// If we exceed the maximum depth, we act like this region doesn't exist
	if(level >= MAX_REGION_LEVEL) { return; }


//    if (level < MIN_REGION_LEVEL || level >= MAX_REGION_LEVEL)
//        return;

    incDynamicRegionId(regionId);
    versions[level]++;
/*   
	UInt64 parentSid = (level > 0) ? regionInfo[level-1].regionId : 0;
    UInt64 parentDid = (level > 0) ? regionInfo[level-1].did : 0;
    if (regionType < 2)
        MSG(0, "[+++] region [%u, %d, %llu:%llu] parent [%llu:%llu] start: %llu\n",
            regionType, level, regionId, getDynamicRegionId(regionId), 
            parentSid, parentDid, timestamp);
*/
    if (regionType < 2)
        MSG(0, "[+++] region [%u, %d, %llu:%llu] start: %llu\n",
            regionType, level, regionId, *getDynamicRegionId(regionId), timestamp);

    // for now, recursive call is not allowed
	/*
    int i;
    for (i=0; i<level; i++) {
        assert(regionInfo[i].regionId != regionId && "For now, no recursive calls!");
    }
	*/

	//fprintf(stderr,"entering region with ID = %llu\n. level = %d",regionId,level);
    regionInfo[level].regionId = regionId;
    regionInfo[level].start = timestamp;
    regionInfo[level].did = *getDynamicRegionId(regionId);
    regionInfo[level].cp = 0LL;
    regionInfo[level].childrenWork = 0LL;
    regionInfo[level].childrenCP = 0LL;

#ifdef EXTRA_STATS
    regionInfo[level].readCnt = 0LL;
    regionInfo[level].writeCnt = 0LL;
    regionInfo[level].readLineCnt = 0LL;
    regionInfo[level].writeLineCnt = 0LL;
#endif

#ifndef USE_UREGION
	UInt64 callSiteId = (funcHead == NULL) ? 0x0 : funcHead->callSiteId;
	cregionPutContext(regionId, callSiteId);
#endif

#ifndef WORK_ONLY
    if (level >= MIN_REGION_LEVEL && level < MAX_REGION_LEVEL)
        setCdt(level, versions[level], 0);
#endif
    incIndentTab();

}

UInt64 _lastSid;
UInt64 _lastDid;
UInt64 _lastWork;
UInt64 _lastCP;
UInt64 _lastStart;
UInt64 _lastEnd;
UInt64 _lastCnt;
UInt64 _lastParentSid;
UInt64 _lastParentDid;

void logRegionExit(UInt64 regionId, UInt regionType) {
    if (!isKremlinOn()) {
        return;
    }

    int level = getCurrentLevel();

	if(level >= MAX_REGION_LEVEL) {
#ifndef WORK_ONLY
		if (regionType == RegionFunc) { 
			popFuncContext();
            FuncContext* funcHead = *FuncContextsLast(funcContexts);
			if (funcHead == NULL) {
				assert(getCurrentLevel() == 0);

			} else {
				setLocalTable(funcHead->table);
			}
#if MANAGE_BB_INFO
			MSG(1, "    currentBB: %u   lastBB: %u\n",
				__currentBB, __prevBB);
#endif
		}
#endif
    	decrementRegionLevel();
		return;
	}

    UInt64 sid = regionId;
    UInt64 did = regionInfo[level].did;
    if(regionInfo[level].regionId != regionId) {
		fprintf(stderr, "mismatch in regionID. expected %llu, got %llu. level = %d\n",regionInfo[level].regionId,regionId,level);
		assert(0);
	}
    UInt64 startTime = regionInfo[level].start;
    UInt64 endTime = timestamp;
    UInt64 work = endTime - startTime;
    UInt64 cp = regionInfo[level].cp;

	if (work == 0 || cp == 0 || work < cp) {
		fprintf(stderr, "sid=%llu work=%llu childrenWork = %llu cp=%llu\n", sid, work, regionInfo[level].childrenWork, cp);
	}

	assert(work == 0 || cp > 0);
	assert(work >= cp);
	assert(work >= regionInfo[level].childrenWork);

	double spTemp = (work - regionInfo[level].childrenWork + regionInfo[level].childrenCP) / (double)cp;
	double sp = (work > 0) ? spTemp : 1.0;
	if(sp < 1.0) {
		fprintf(stderr, "sid=%lld work=%llu childrenWork = %llu childrenCP=%lld\n", sid, work, regionInfo[level].childrenWork, regionInfo[level].childrenCP);
		assert(0);
	}
	//assert(sp >= 1.0);
	//assert(cp >= 1.0);

	UInt64 spWork = (UInt64)((double)work / sp);
	UInt64 tpWork = cp;
	//fprintf(stderr, "work=%llu childrenWork = %llu childrenCP=%lld cp=%lld sp=%.2f spTemp=%.2f\n", work, regionInfo[level].childrenWork, regionInfo[level].childrenCP, cp, sp, spTemp);

	// due to floating point variables,
	// spWork or tpWork can be larger than work
	if (spWork > work)
		spWork = work;
	if (tpWork > work)
		tpWork = work;

    if(regionId != regionInfo[level].regionId) {
        fprintf(stderr,"ERROR: unexpected region exit: %llu (expected region %llu)\n",regionId,regionInfo[level].regionId);
        assert(0);
    }
	UInt64 parentSid = 0;
	UInt64 parentDid = 0;

	if (level > 0) {
    	parentSid = regionInfo[level-1].regionId;
		parentDid = regionInfo[level-1].did;
		regionInfo[level-1].childrenWork += work;
		regionInfo[level-1].childrenCP += cp;
	} 

    if(work < cp) {
        fprintf(stderr,"ERROR: cp (%llu) > work (%llu) [regionId=%llu]",cp,work,regionId);
        assert(0);
    }

    decIndentTab();
    /*
    if (regionType < 2)
        MSG(0, "[---] region [%u, %u, %llu:%llu] parent [%llu:%llu] cp %llu work %llu\n",
                regionType, level, regionId, did, parentSid, parentDid, 
                regionInfo[level].cp, work);
                */

    if (regionType < 2)
        MSG(0, "[---] region [%u, %u, %llu:%llu] cp %llu work %llu\n",
                regionType, level, regionId, did, regionInfo[level].cp, work);

    if (isKremlinOn() && work > 0 && cp == 0 && isCurrentLevelInstrumentable()) {
        fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
        fprintf(stderr, "region [type: %u, level: %u, id: %llu:%llu] parent [%llu:%llu] cp %llu work %llu\n",
            regionType, level, regionId, did, parentSid, parentDid, 
            regionInfo[level].cp, work);
        assert(0);
    }

    RegionField field;
    field.work = work;
    field.cp = cp;
	field.callSite = (*FuncContextsLast(funcContexts))->callSiteId;
	field.spWork = spWork;
	field.tpWork = tpWork;
	assert(work >= spWork);
	assert(work >= tpWork);
#ifdef EXTRA_STATS
    field.readCnt = regionInfo[level].readCnt;
    field.writeCnt = regionInfo[level].writeCnt;
    field.readLineCnt = regionInfo[level].readLineCnt;
    field.writeLineCnt = regionInfo[level].writeLineCnt;
    assert(work >= field.readCnt && work >= field.writeCnt);
#endif

#ifdef USE_UREGION
    processUdr(sid, did, parentSid, parentDid, field);
#else
	cregionRemoveContext(&field);
#endif
        
#ifndef WORK_ONLY
    if (regionType == RegionFunc) { 
        popFuncContext();
        FuncContext* funcHead = *FuncContextsLast(funcContexts);
        if (funcHead == NULL) {
            assert(getCurrentLevel() == 0);

        } else {
            setLocalTable(funcHead->table);
        }
#if MANAGE_BB_INFO
        MSG(1, "    currentBB: %u   lastBB: %u\n",
            __currentBB, __prevBB);
#endif
    }
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
    TEntry* entry0 = getLTEntry(src0);
    TEntry* entry1 = getLTEntry(src1);
    TEntry* entryDest = getLTEntry(dest);

    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; ++i) {
		// calculate availability time
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);
        UInt64 ts0 = getTimestamp(entry0, i, version);
        UInt64 ts1 = getTimestamp(entry1, i, version);
        UInt64 greater0 = (ts0 > ts1) ? ts0 : ts1;
        UInt64 greater1 = (cdt > greater0) ? cdt : greater0;
        UInt64 value = greater1 + opCost;
        updateTimestamp(entryDest, i, version, value);
        UInt64 cp = updateCP(value, i);
		
        MSG(2, "binOp[%u] level %u version %u \n", opCost, i, version);
        MSG(2, " src0 %u src1 %u dest %u\n", src0, src1, dest);
        MSG(2, " ts0 %u ts1 %u cdt %u value %u\n", ts0, ts1, cdt, value);
    }

    return entryDest;
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
    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    assert(funcHead->table != NULL); // TODO: is this /really/ necessary here?

    TEntry* entry0 = getLTEntry(src);
    TEntry* entryDest = getLTEntry(dest);
    
    //assert(entry0 != NULL);
    //assert(entryDest != NULL);

    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
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
#else
    return NULL;
#endif
}


void* logAssignment(UInt src, UInt dest) {
    if (!isKremlinOn())
        return NULL;
    
    return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
    MSG(1, "logAssignmentConst ts[%u]\n", dest);

#ifndef WORK_ONLY
    TEntry* entryDest = getLTEntry(dest);
    
    //assert(entryDest != NULL);

    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);

        updateTimestamp(entryDest, i, version, cdt);
        updateCP(cdt, i);
    }

    return entryDest;
#else
    return NULL;
#endif
}

void* logLoadInst(Addr src_addr, UInt dest) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "load ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOAD_COST);
    addWork(LOAD_COST);
    //addLoad();

#ifndef WORK_ONLY
    TEntry* entry0 = getGTEntry(src_addr);
    TEntry* entryDest = getLTEntry(dest);

#ifdef EXTRA_STATS
    TEntry* entry0Line = getGTEntryCacheLine(src_addr);
#endif

    //assert(entryDest != NULL);
    //assert(entry0 != NULL);
    
    //fprintf(stderr, "\n\nload ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOAD_COST);
    //fprintf(stderr, "load addr = 0x%x, entryLine = 0x%x\n", src_addr, entry0Line);
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);
        UInt64 ts0 = getTimestamp(entry0, i, version);
        UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
        UInt64 value = greater1 + LOAD_COST;

#ifdef EXTRA_STATS
        updateReadMemoryAccess(entry0, i, version, value);
        updateReadMemoryLineAccess(entry0Line, i, version, value);
#endif

        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
    }

    return entryDest;
#else
    return NULL;
#endif
}

void* logStoreInst(UInt src, Addr dest_addr) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "store ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);

    addWork(STORE_COST);
    //addStore();

#ifndef WORK_ONLY
    TEntry* entry0 = getLTEntry(src);
    TEntry* entryDest = getGTEntry(dest_addr);

#ifdef EXTRA_STATS
    TEntry* entryLine = getGTEntryCacheLine(dest_addr);
#endif

    assert(entryDest != NULL);
    assert(entry0 != NULL);

    //fprintf(stderr, "\n\nstore ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 cdt = getCdt(i,version);
        UInt64 ts0 = getTimestamp(entry0, i, version);
        UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
        UInt64 value = greater1 + STORE_COST;

#ifdef EXTRA_STATS
        updateWriteMemoryAccess(entryDest, i, version, value);
        updateWriteMemoryLineAccess(entryLine, i, version, value);
#endif

        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
    }

    return entryDest;
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
    TEntry* entryDest = getGTEntry(dest_addr);
    assert(entryDest != NULL);

#ifdef EXTRA_STATS
    TEntry* entryLine = getGTEntryCacheLine(dest_addr);
#endif
    
    //fprintf(stderr, "\nstoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; ++i) {
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
#else
    return NULL;
#endif
}

// TODO: 64 bit?
void logMalloc(Addr addr, size_t size, UInt dest) {
    if (!isKremlinOn())
        return;
    
    MSG(1, "logMalloc addr=0x%x size=%llu\n", addr, (UInt64)size);
	assert(regionInfo[0].start == 0ULL);

#ifndef WORK_ONLY
    assert(size != 0);
    UInt32 start_index, end_index;
    start_index = ((UInt64) addr >> 16) & 0xffff;
    UInt64 end_addr = (UInt64)addr + (size-1);
    end_index = (end_addr >> 16) & 0xffff;

    //MSG(1,"start_index = %lu, end_index = %lu\n",start_index,end_index);

    if(start_index == end_index) {
        // get/create entry and set "used" field appropriately
        GEntry* entry = gTable->array[start_index];
        if (entry == NULL) {
            entry = createGEntry();
            gTable->array[start_index] = entry;
            entry->used = (size >> 2);
        }
        else {
            entry->used += (size >> 2);
        }

        MSG(2,"  allocating from gTable[%lu]. %hu used in this block\n",start_index,entry->used);

        UInt32 start_index2, end_index2;

        // find starting and ending addr
        start_index2 = ((UInt64) addr >> 2) & 0x3fff;
        end_index2 = (end_addr >> 2) & 0x3fff;

        MSG(2,"  gTable entry range: [%lu:%lu]\n",start_index2,end_index2);

        // create TEntry instances for the range of mem addrs
        // We assume that TEntry instances don't exist for the
        // index2 range because otherwise malloc would be buggy.
        int i;
        for(i = start_index2; i <= end_index2; ++i) {
            entry->array[i] = allocTEntry(maxRegionLevel);
        }

    }
    else {
        // handle start_index
        GEntry* entry = gTable->array[start_index];
        UInt32 start_index2 = ((UInt64) addr >> 2) & 0x3fff;

        if (entry == NULL) {
            entry = createGEntry();
            gTable->array[start_index] = entry;
            entry->used = (0x4000-start_index2);
        }
        else {
            entry->used += (0x4000-start_index2);
        }

        MSG(2,"  allocating from gTable[%lu]. %hu used in this block\n",start_index,entry->used);

        int i;
        for(i = start_index2; i < 0x4000; ++i) {
            entry->array[i] = allocTEntry(maxRegionLevel);
        }

        // handle end_index
        entry = gTable->array[end_index];
        UInt32 end_index2 = (end_addr >> 2) & 0x3fff;

        if (entry == NULL) {
            entry = createGEntry();
            gTable->array[end_index] = entry;
            entry->used = end_index2 + 1;
        }
        else {
            entry->used += (end_index2+1);
        }

        for(i = 0; i <= end_index2; ++i) {
            entry->array[i] = allocTEntry(maxRegionLevel);
        }

        MSG(2,"  allocating from gTable[%lu]. %hu used in this block\n",end_index,entry->used);

        // handle all intermediate indices
        UInt32 curr_index;
        for(curr_index = start_index+1; curr_index < end_index; ++curr_index) {
            // assume that malloc isn't buggy and therefore won't give us addresses
            // that have been used but not freed
            entry = createGEntry();
            gTable->array[curr_index] = entry;
            entry->used = 0x4000;

            MSG(2,"  allocating all entries in gTable[%lu].\n",curr_index);

            for(i = 0; i < 0x4000; ++i) {
                entry->array[i] = allocTEntry(maxRegionLevel);
            }
        }
    }

    createMEntry(addr,size);
	if (regionInfo[0].start != 0ULL) {
		fprintf(stderr, "add regionInfo[0] = 0x%x\n", &regionInfo[0]);
	}
/*
	addWork(MALLOC_COST);

	// update timestamp and CP
    TEntry* entryDest = getLTEntry(dest);
    
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 value = getCdt(i,version) + MALLOC_COST;

        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
    }
	*/
#endif
}

void logFree(Addr addr) {
    if (!isKremlinOn())
        return;
    MSG(1, "logFree addr=0x%x\n", addr);

    // Calls to free with NULL just return.
    if(addr == NULL)
        return;


#ifndef WORK_ONLY
    MEntry* me = getMEntry(addr);

    size_t mem_size = me->size;

    UInt32 start_index, end_index;

    start_index = ((UInt64) addr >> 16) & 0xffff;

    UInt64 end_addr = (UInt64)addr + (mem_size-1);

    end_index = (end_addr >> 16) & 0xffff;

    if(start_index == end_index) {
        // get entry (must exist b/c of logMalloc)
        GEntry* entry = gTable->array[start_index];
        assert(entry != NULL);

        entry->used -= (mem_size >> 2);

        MSG(2,"  freeing from gTable[%lu]. %hu entries remain in use\n",start_index,entry->used);

        UInt32 start_index2, end_index2;

        // find starting and ending addr
        start_index2 = ((UInt64) addr >> 2) & 0x3fff;
        end_index2 = (end_addr >> 2) & 0x3fff;

        MSG(2,"  gTable entry range: [%lu:%lu]\n",start_index2,end_index2);

        // free TEntry instances for the range of mem addrs
        int i;
        for(i = start_index2; i <= end_index2; ++i) {
            freeTEntry(entry->array[i]);
            entry->array[i] = NULL;
        }

        // if nothing in this gtable entry is used
        // then we can safely free it
        if(entry->used == 0) { 
            free(entry);
            gTable->array[start_index] = NULL;
            MSG(2,"    freeing gTable entry.\n");
        }
    }
    else {
        // handle start_index
        GEntry* entry = gTable->array[start_index];
        UInt32 start_index2 = ((UInt64) addr >> 2) & 0x3fff;

        int i;
        for(i = start_index2; i < 0x4000; ++i) {
            freeTEntry(entry->array[i]);
            entry->array[i] = NULL;
        }

        entry->used -= (0x4000-start_index2);

        MSG(2,"  freeing from gTable[%lu] (range: [%lu:0x4000]). %hu entries remain in use\n",start_index,start_index2,entry->used);

        // free it if nothing used
        if(entry->used == 0) { 
            free(entry);
            gTable->array[start_index] = NULL;
            MSG(2,"    freeing gTable entry.\n");
        }

        // handle end_index
        entry = gTable->array[end_index];
        UInt32 end_index2 = (end_addr >> 2) & 0x3fff;

        for(i = 0; i <= end_index2; ++i) {
            freeTEntry(entry->array[i]);
            entry->array[i] = NULL;
        }

        entry->used -= (end_index2+1);

        MSG(2,"  freeing from gTable[%lu] (range: [0:%lu]). %hu entries remain in use\n",end_index,entry->used);

        // free it if nothing used
        if(entry->used == 0) { 
            free(entry);
            gTable->array[end_index] = NULL;
            MSG(2,"    freeing gTable entry.\n");
        }

        // handle all intermediate indices
        UInt32 curr_index;
        for(curr_index = start_index+1; curr_index < end_index; ++curr_index) {
            entry = gTable->array[curr_index];

            MSG(2,"  freeing all entries from gTable[%lu].\n",curr_index);

            for(i = 0; i < 0x4000; ++i) {
                freeTEntry(entry->array[i]);
                // we don't need to set entry->array[i] to NULL here since we are
                // nullifying the whole entry
            }

            // intermediate will always have all TEntries
            // deleted and therefore are always safe to free
            free(entry);
            gTable->array[curr_index] = NULL;
        }
    }

    freeMEntry(addr);

	addWork(FREE_COST);

	// make sure CP is at least the time needed to complete the free
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 value = getCdt(i,version) + FREE_COST;

        updateCP(value, i);
    }
#endif
}

// TODO: more efficient implementation (if old_addr = new_addr)
void logRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest) {
    if (!isKremlinOn())
        return;

    MSG(1, "logRealloc old_addr=0x%x new_addr=0x%x size=%llu\n", old_addr, new_addr, (UInt64)size);
    logFree(old_addr);
    logMalloc(new_addr,size,dest);
}

void* logInsertValue(UInt src, UInt dest) {
    printf("Warning: logInsertValue not correctly implemented\n");
    return logAssignment(src, dest);
}

void* logInsertValueConst(UInt dest) {
    printf("Warning: logInsertValueConst not correctly implemented\n");
    return logAssignmentConst(dest);
}

#define CDTSIZE	256
static CDT* cdtPool;
static int cdtSize;
static int cdtIndex;

CDT* allocCDT() {
	CDT* ret = &cdtPool[cdtIndex++];
	if (cdtIndex == cdtSize) {
		int i;
		cdtSize += CDTSIZE;
		cdtPool = realloc(cdtPool, sizeof(CDT) * cdtSize);
		for (i=cdtSize-CDTSIZE; i<cdtSize; i++) {
    		cdtPool[i].time = (UInt64*) calloc(getTEntrySize(), sizeof(UInt64));
		    cdtPool[i].version = (UInt32*) calloc(getTEntrySize(), sizeof(UInt32));
		}
	};
	return ret;
}

void freeCDT(CDT* toFree) {
	cdtIndex--;
}

void initCDT() {
	int i=0;
	cdtPool = malloc(sizeof(CDT) * CDTSIZE);
	cdtSize = CDTSIZE;

	for (i=0; i<CDTSIZE; i++) {
    	cdtPool[i].time = (UInt64*) calloc(getTEntrySize(), sizeof(UInt64));
	    cdtPool[i].version = (UInt32*) calloc(getTEntrySize(), sizeof(UInt32));
	}
	cdtHead = allocCDT();
}

void deinitCDT() {
	int i=0;
	for (i=0; i<cdtSize; i++) {
		free(cdtPool[i].time);
		free(cdtPool[i].version);
	}
}

void addControlDep(UInt cond) {
    MSG(2, "push ControlDep ts[%u]\n", cond);

#ifndef WORK_ONLY
    cdtHead = allocCDT();
    if (isKremlinOn()) {
        TEntry* entry = getLTEntry(cond);
        fillCDT(cdtHead, entry);
    }
#endif
}

void removeControlDep() {
    MSG(2, "pop  ControlDep\n");
#ifndef WORK_ONLY
	freeCDT(cdtHead);
    cdtHead--;
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
void logFuncReturn(UInt src) {
    if (!isKremlinOn())
        return;
    MSG(1, "write return value ts[%u]\n", src);

#ifndef WORK_ONLY
    TEntry* srcEntry = getLTEntry(src);

    // Assert there is a function context before the top.
    assert(FuncContextsSize(funcContexts) > 1);

    // Assert that its return value has been set
    FuncContext** nextHead = FuncContextsLast(funcContexts) - 1;
    assert((*nextHead)->ret);

    // Copy the return timestamp into the previous stack's return value.
    copyTEntry((*nextHead)->ret, srcEntry);
#endif
}

void logFuncReturnConst(void) {
    if (!isKremlinOn())
        return;
    MSG(1, "logFuncReturnConst\n");

#ifndef WORK_ONLY
    int i;
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    // Assert there is a function context before the top.
    assert(FuncContextsSize(funcContexts) > 1);

    FuncContext** nextHead = FuncContextsLast(funcContexts) - 1;

    for (i = minLevel; i < maxLevel; i++) {
        int version = getVersion(i);
        UInt64 cdt = getCdt(i,version);

        // Copy the return timestamp into the previous stack's return value.
        updateTimestamp((*nextHead)->ret, i, version, cdt);
//      funcHead->ret->version[i] = version;
//      funcHead->ret->time[i] = cdt;
    }
#endif
}

void logBBVisit(UInt bb_id) {
    if (!isKremlinOn())
        return;
#ifdef MANAGE_BB_INFO
    MSG(1, "logBBVisit(%u)\n", bb_id);
    __prevBB = __currentBB;
    __currentBB = bb_id;
#endif
}

void* logPhiNode1CD(UInt dest, UInt src, UInt cd) {
    if (!isKremlinOn())
        return NULL;
    MSG(1, "logPhiNode1CD ts[%u] = max(ts[%u], ts[%u])\n", dest, src, cd);

#ifndef WORK_ONLY
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD = getLTEntry(cd);
    TEntry* entryDest = getLTEntry(dest);

    assert(entrySrc != NULL);
    assert(entryCD != NULL);
    assert(entryDest != NULL);
    
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
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
#else
    return NULL;
#endif
}

void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "logPhiNode2CD ts[%u] = max(ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2);

#ifndef WORK_ONLY
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryDest = getLTEntry(dest);

    assert(entrySrc != NULL);
    assert(entryCD1 != NULL);
    assert(entryCD2 != NULL);
    assert(entryDest != NULL);
    
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
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
#else
    return NULL;
#endif
}
void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3) {
    if (!isKremlinOn())
        return NULL;
    MSG(1, "logPhiNode3CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3);

#ifndef WORK_ONLY
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
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    for (i = minLevel; i < maxLevel; i++) {
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
#else
    return NULL;
#endif
}

void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "logPhiNode4CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryCD3 = getLTEntry(cd3);
    TEntry* entryCD4 = getLTEntry(cd4);
    TEntry* entryDest = getLTEntry(dest);

    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    assert(funcHead->table->size > src);
    assert(funcHead->table->size > cd1);
    assert(funcHead->table->size > cd2);
    assert(funcHead->table->size > cd3);
    assert(funcHead->table->size > cd4);
    assert(funcHead->table->size > dest);
    assert(entrySrc != NULL);
    assert(entryCD1 != NULL);
    assert(entryCD2 != NULL);
    assert(entryCD3 != NULL);
    assert(entryCD4 != NULL);
    assert(entryDest != NULL);
    
    int i = 0;
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    for (i = minLevel; i < maxLevel; i++) {
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
#else
    return NULL;
#endif
}

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "log4CDToPhiNode ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, dest, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryCD3 = getLTEntry(cd3);
    TEntry* entryCD4 = getLTEntry(cd4);
    TEntry* entryDest = getLTEntry(dest);

    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    assert(funcHead->table->size > cd1);
    assert(funcHead->table->size > cd2);
    assert(funcHead->table->size > cd3);
    assert(funcHead->table->size > cd4);
    assert(funcHead->table->size > dest);
    assert(entryCD1 != NULL);
    assert(entryCD2 != NULL);
    assert(entryCD3 != NULL);
    assert(entryCD4 != NULL);
    assert(entryDest != NULL);
    
    int i = 0;
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    for (i = minLevel; i < maxLevel; i++) {
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
#else
    return NULL;
#endif
}

#define MAX_ENTRY 10

void logPhiNodeAddCondition(UInt dest, UInt src) {
    if (!isKremlinOn())
        return;
    MSG(1, "logPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest, src, dest);

#ifndef WORK_ONLY
    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());
    int i = 0;
    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    assert(funcHead->table != NULL);
    assert(funcHead->table->size > src);
    assert(funcHead->table->size > dest);
    TEntry* entry0 = getLTEntry(src);
    TEntry* entry1 = getLTEntry(dest);
    TEntry* entryDest = entry1;
    
    assert(entry0 != NULL);
    assert(entry1 != NULL);
    assert(entryDest != NULL);

    for (i = minLevel; i < maxLevel; i++) {
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
#endif
}

// use estimated cost for a callee function we cannot instrument
void* logLibraryCall(UInt cost, UInt dest, UInt num_in, ...) { 
    if (!isKremlinOn())
        return NULL;

    MSG(1, "logLibraryCall to ts[%u] with cost %u\n", dest, cost);
    addWork(cost);

#ifndef WORK_ONLY
    TEntry* srcEntry[MAX_ENTRY];
    TEntry* destEntry = getLTEntry(dest);

    va_list ap;
    va_start(ap, num_in);

    int i;
    for (i = 0; i < num_in; i++) {
        UInt src = va_arg(ap, UInt);
        srcEntry[i] = getLTEntry(src);
        assert(srcEntry[i] != NULL);
    }   
    va_end(ap);

    int minLevel = MIN_REGION_LEVEL;
    int maxLevel = MIN(MAX_REGION_LEVEL, getLevelNum());

    for (i = minLevel; i < maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 max = 0;
        
        int j;
        for (j = 0; j < num_in; j++) {
            UInt64 ts = getTimestamp(srcEntry[j], i, version);
            if (ts > max)
                max = ts;
        }   
        
		UInt64 value = max + cost;

        updateTimestamp(destEntry, i, version, value);
		updateCP(value, i);
    }
    return destEntry;
#else
    return NULL;
#endif
    
}

// this function is the same as logAssignmentConst but helps to quickly
// identify induction variables in the source code
void* logInductionVar(UInt dest) {
    if (!isKremlinOn())
        return NULL;
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

    fprintf(stderr,"DEBUGLEVEL = %d\n", DEBUGLEVEL);

#ifdef USE_UREGION
    initializeUdr();
#else
	cregionInit();
#endif
    levelNum = 0;
    InvokeRecordsCreate(&invokeRecords);
    int storageSize = MAX_REGION_LEVEL - MIN_REGION_LEVEL;
    MSG(0, "minLevel = %d maxLevel = %d storageSize = %d\n", 
        _minRegionToLog, _maxRegionToLog, storageSize);

    // Allocate a memory allocator.
    MemMapAllocatorCreate(&memPool, ALLOCATOR_SIZE);

    // Emulates the call stack.
    FuncContextsCreate(&funcContexts);
    initDataStructure(storageSize);

    versions = (int*) calloc(sizeof(int), _MAX_REGION_LEVEL);
    assert(versions);

    regionInfo = (Region*) calloc(sizeof(Region), _MAX_REGION_LEVEL);
    assert(regionInfo);

    // Allocate a deque to hold timestamps of args.
    deque_create(&argTimestamps, NULL, NULL);

    // Allocate the hash map to store dynamic region id counts.
    hash_map_sid_did_create(&sidToDid, sidHash, sidCompare, NULL, NULL);

    allocDummyTEntry();

	initCDT();
    initStartFuncContext();
    
	turnOnProfiler();
    return TRUE;
}

void deinitStartFuncContext()
{
	popFuncContext();
}

void initStartFuncContext()
{
    prepareCall(0, 0);
	pushFuncContext();

    (*FuncContextsLast(funcContexts))->ret = allocTEntry(getTEntrySize());
}


int kremlinDeinit() {
    if(--hasInitialized) {
        MSG(0, "kremlinDeinit skipped\n");
        return FALSE;
    }
    MSG(0, "kremlinDeinit running\n");

	turnOffProfiler();

#ifdef USE_UREGION
    finalizeUdr();
#else
	cregionFinish("kremlin.bin");
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

    deinitStartFuncContext();

    cdtHead = NULL;

    // Deallocate the memory allocator.
    MemMapAllocatorDelete(&memPool);

    // Emulates the call stack.
    FuncContextsDelete(&funcContexts);
	deinitCDT();


    fprintf(stderr, "[kremlin] minRegionLevel = %d maxRegionLevel = %d\n", 
        _minRegionToLog, _maxRegionToLog);
    fprintf(stderr, "[kremlin] app MaxRegionLevel = %d\n", _maxRegionNum);

    kremlinOn = FALSE;

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

int sidCompare(UInt64 s1, UInt64 s2) {
    return s1 == s2;
}

void printProfileData() {}

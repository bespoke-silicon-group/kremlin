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

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)

#define _MAX_REGION_LEVEL   100     // used for static data structures

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))

UInt64*     getDynamicRegionId(UInt64 sid);
void        incDynamicRegionId(UInt64 sid);
UInt64      sidHash(UInt64 sid);
int         sidCompare(UInt64 s1, UInt64 s2);

HASH_MAP_DEFINE_PROTOTYPES(sid_did, UInt64, UInt64);
HASH_MAP_DEFINE_FUNCTIONS(sid_did, UInt64, UInt64);

typedef struct _CDT_T {
    UInt64* time;
    UInt32* version;
    struct _CDT_T* next;
} CDT;

typedef struct _FuncContext {
    LTable* table;
    TEntry* ret;
	UInt64 callSiteId;
#ifdef MANAGE_BB_INFO
    UInt    retBB;
    UInt    retPrevBB;
#endif
    struct _FuncContext* next;
    
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
    UInt id;
    int stackHeight;
} InvokeRecord;

#if 1
const int               _maxRegionToLog = MAX_REGION_LEVEL;
const int               _minRegionToLog = MIN_REGION_LEVEL;
#endif

int                 pyrprofOn = 0;
int                 levelNum = 0;
int*                versions = NULL;
Region*             regionInfo = NULL;
CDT*                cdtHead = NULL;
FuncContext*        funcHead = NULL;
InvokeRecord        invokeStack[_MAX_REGION_LEVEL];
InvokeRecord*       invokeStackTop;
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

#define isPyrprofOn()       (pyrprofOn == 1)
#define getLevelNum()      (levelNum)
#define getCurrentLevel()  (levelNum-1)
#define isCurrentLevelInstrumentable() (((levelNum-1) >= _minRegionToLog) && ((levelNum-1) <= _maxRegionToLog))

UInt64 _regionEntryCnt;
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
 * when pyrprofOn == 0,
 * most instrumentation functions do nothing.
 */ 
void turnOnProfiler() {
	fprintf(stderr, "turnOnProfiler...");
    pyrprofOn = 1;
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
    pyrprofOn = 0;
	fprintf(stderr, "turnOffProfiler\n");
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
    // Clear off any argument timestamps that have been left here before the
    // call. These are left on the deque because library calls never take
    // theirs off. 
    deque_clear(argTimestamps);
	lastCallSiteId = callSiteId;
}

void linkArgToLocal(UInt src) {
    if (!isPyrprofOn())
        return;
    MSG(1, "linkArgToLocal to ts[%u]\n", src);

#ifndef WORK_ONLY
    deque_push_back(argTimestamps, getLTEntry(src));
#endif
}

// special case for constant arg
void linkArgToConst() {
    if (!isPyrprofOn())
        return;
    MSG(1, "linkArgToConst\n");

#ifndef WORK_ONLY
    deque_push_back(argTimestamps, getDummyTEntry());
#endif
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(UInt dest) {
    if (!isPyrprofOn())
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

CDT* allocCDT() {
    CDT* ret = (CDT*) malloc(sizeof(CDT));
    ret->time = (UInt64*) malloc(sizeof(UInt64) * getTEntrySize());
    ret->version = (UInt32*) malloc(sizeof(UInt32) * getTEntrySize());
    bzero(ret->time, sizeof(UInt64) * getTEntrySize());
    bzero(ret->version, sizeof(UInt32) * getTEntrySize());
    ret->next = NULL;
    return ret;
}

void freeCDT(CDT* cdt) {
    free(cdt->time);
    free(cdt->version);
    free(cdt);  
}

void setupLocalTable(UInt maxVregNum) {
    MSG(1, "setupLocalTable size %u\n", maxVregNum);

#ifndef WORK_ONLY
    assert(_requireSetupTable == 1);
    LTable* table = allocLocalTable(maxVregNum);
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
    return (cdtHead->version[level - _minRegionToLog] == version) ?
        cdtHead->time[level - _minRegionToLog] : 0;
}

void setCdt(int level, UInt32 version, UInt64 time) {
    assert(level >= _minRegionToLog);
    cdtHead->time[level - _minRegionToLog] = time;
    cdtHead->version[level - _minRegionToLog] = version;
}

void fillCDT(CDT* cdt, TEntry* entry) {
    int i;
    for (i = _minRegionToLog; i <= _maxRegionToLog; i++) {
        cdt->time[i - _minRegionToLog] = entry->time[i - _minRegionToLog];
        cdt->version[i - _minRegionToLog] = entry->version[i - _minRegionToLog];
    }
}

void pushFuncContext() {
    FuncContext* prevHead = funcHead;
    FuncContext* toAdd = (FuncContext*) malloc(sizeof(FuncContext));
    toAdd->table = NULL;
	toAdd->callSiteId = lastCallSiteId;
    toAdd->next = prevHead;
#ifdef MANAGE_BB_INFO
    toAdd->retBB = __currentBB;
    toAdd->retPrevBB = __prevBB;
    MSG(1, "[push] current = %u last = %u\n", __currentBB, __prevBB);
#endif
    funcHead = toAdd;
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


/*
void dumpRegion() {
#if 0
    int i;
    for (i = 0; i < getLevelNum(); i++) {
        fprintf(stderr, "%d: %lld\t%lld\n", 
            i, regionInfo[i].start, regionInfo[i].end);
    }
#endif
}

void incDynamicRegionId(UInt64 sid) {
    dynamicRegionId[sid]++;
    int i;
}

UInt64 getDynamicRegionId(UInt64 sid) {
    return dynamicRegionId[sid];
}
*/


UInt64 updateCP(UInt64 value, int level) {
	regionInfo[level].cp = (value > regionInfo[level].cp) ? value : regionInfo[level].cp;
	return regionInfo[level].cp;

    //MSG(1,"CP[%d] = %llu\n",level,regionInfo[level].cp);
}

#define addWork(work) timestamp += work;

void addLoad() {
    loadCnt++;
}

void addStore() {
    storeCnt++;
}

void popFuncContext() {
    FuncContext* ret = funcHead;
    assert(ret != NULL);
    //assert(ret->table != NULL);
    // restore currentBB and prevBB
#ifdef MANAGE_BB_INFO
    __currentBB = ret->retBB;
    __prevBB = ret->retPrevBB;
#endif
    funcHead = ret->next;
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

// precondition: inLevel >= _minRegionToLog && inLevel < maxRegionSize
UInt64 getTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
    int level = inLevel - _minRegionToLog;

    UInt64 ret = (entry->version[level] == version) ?
                    entry->time[level] : 0;
    return ret;
}

// precondition: entry != NULL
void updateTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int level = inLevel - _minRegionToLog;

    entry->version[level] = version;
    entry->time[level] = timestamp;
}

#ifdef EXTRA_STATS
UInt64 getReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
    int level = inLevel - _minRegionToLog;
    assert(entry != NULL);
    assert(level >= 0 && level < getTEntrySize());
    UInt64 ret = (entry->readVersion[level] == version) ?
                    entry->readTime[level] : 0;
    return ret;
}

void updateReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int level = inLevel - _minRegionToLog;
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


#ifdef __cplusplus
void invokeAssert() {
    assert(invokeStackTop >= invokeStack);
    assert(invokeStackTop < invokeStack + _MAX_REGION_LEVEL);
}

void prepareInvoke(UInt id) {
    if(!instrument)
        return;
    MSG(1, "prepareInvoke(%u) - saved at %d\n", id, instrument);

    invokeAssert();

    InvokeRecord* currentRecord = invokeStackTop++;
    currentRecord->id = id;
    currentRecord->stackHeight = instrument;

    invokeAssert();
}

void invokeOkay(UInt id) {
    if(!instrument)
        return;

    invokeAssert();
    if(invokeStackTop > invokeStack && (invokeStackTop - 1)->id == id) {
        MSG(1, "invokeOkay(%u)\n", id);
        invokeStackTop--;

        invokeAssert();
    } else
        MSG(1, "invokeOkay(%u) ignored\n", id);
}
void invokeThrew(UInt id)
{
    if(!instrument)
        return;

    invokeAssert();

    if(invokeStackTop > invokeStack && (invokeStackTop - 1)->id == id) {
        InvokeRecord* currentRecord = invokeStackTop - 1;
        MSG(1, "invokeThrew(%u) - Popping to %d\n", currentRecord->id, currentRecord->stackHeight);

        int lastInstrument = instrument;
        while(instrument > currentRecord->stackHeight)
        {
            int level = getCurrentLevel();
            logRegionExit(regionInfo[level].regionId, 0);

            assert(instrument < lastInstrument);
            lastInstrument = instrument;
        }
        invokeStackTop--;
        
        invokeAssert();
    }
    else
        MSG(1, "invokeThrew(%u) ignored\n", id);
}
#endif

void logRegionEntry(UInt64 regionId, UInt regionType) {
    if (regionType == 0)
        _regionFuncCnt++;

    if (!isPyrprofOn()) {
        return;
    }

    if(regionType == RegionFunc)
    {
        pushFuncContext();
        _requireSetupTable = 1;
    }

    incrementRegionLevel();
    _regionEntryCnt++;
    int level = getCurrentLevel();

//    if (level < _minRegionToLog || level > _maxRegionToLog)
//        return;

    incDynamicRegionId(regionId);
    versions[level]++;
/*   
	UInt64 parentSid = (level > 0) ? regionInfo[level-1].regionId : 0;
    UInt64 parentDid = (level > 0) ? regionInfo[level-1].did : 0;
*/
    if (regionType < 2)
        MSG(0, "[+++] level [%u, %d, %llu:%llu] parent [%llu:%llu] start: %llu\n",
            regionType, level, regionId, getDynamicRegionId(regionId), 
            parentSid, parentDid, timestamp);

    // for now, recursive call is not allowed
	/*
    int i;
    for (i=0; i<level; i++) {
        assert(regionInfo[i].regionId != regionId && "For now, no recursive calls!");
    }
	*/

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
    if (level >= _minRegionToLog && level <= _maxRegionToLog)
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
    if (!isPyrprofOn()) {
        if (regionId == RegionFunc) { 
            popFuncContext();


            if (funcHead == NULL) {
                assert(getCurrentLevel() <= 0);
    
            } else {
                setLocalTable(funcHead->table);
            }
        }
        return;
    }
    int level = getCurrentLevel();

    UInt64 sid = regionId;
    UInt64 did = regionInfo[level].did;
    assert(regionInfo[level].regionId == regionId);
    UInt64 startTime = regionInfo[level].start;
    UInt64 endTime = timestamp;
    UInt64 work = endTime - startTime;
    UInt64 cp = regionInfo[level].cp;

	if (work == 0 || cp == 0) {
		fprintf(stderr, "sid=%lld work=%llu childrenWork = %llu cp=%lld\n", sid, work, regionInfo[level].childrenWork, cp);
	}

	assert(work > 0);
	assert(cp > 0);
	assert(work >= cp);
	assert(work >= regionInfo[level].childrenWork);
	double sp = (work - regionInfo[level].childrenWork + regionInfo[level].childrenCP) / (double)cp;

	assert(sp >= 1.0);
	assert(cp >= 1.0);

	UInt64 spWork = (UInt64)((double)work / sp);
	UInt64 tpWork = (UInt64)((double)work / (double)cp);

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
    if (regionType < 2)
        MSG(0, "[---] region [%u, %u, %llu:%llu] parent [%llu:%llu] cp %llu work %llu\n",
                regionType, level, regionId, did, parentSid, parentDid, 
                regionInfo[level].cp, work);
    if (isPyrprofOn() && work > 0 && cp == 0 && isCurrentLevelInstrumentable()) {
        fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
        fprintf(stderr, "region [type: %u, level: %u, id: %llu:%llu] parent [%llu:%llu] cp %llu work %llu\n",
            regionType, level, regionId, did, parentSid, parentDid, 
            regionInfo[level].cp, work);
        assert(0);
    }

    RegionField field;
    field.work = work;
    field.cp = cp;
	field.callSite = funcHead->callSiteId;
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

// TODO: implement me
void logLoopIteration() {}

void* logReductionVar(UInt opCost, UInt dest) {
    addWork(opCost);
    return NULL;
}

void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest) {
    if (!isPyrprofOn())
        return NULL;

    MSG(1, "binOp ts[%u] = max(ts[%u], ts[%u]) + %u\n", dest, src0, src1, opCost);
    addWork(opCost);

#ifndef WORK_ONLY
    TEntry* entry0 = getLTEntry(src0);
    TEntry* entry1 = getLTEntry(src1);
    TEntry* entryDest = getLTEntry(dest);

    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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

		// update timestamp
        updateTimestamp(entryDest, i, version, value);

		// update cp
		//UInt64 oldcp = regionInfo[i].cp;
        UInt64 cp = updateCP(value, i);

    	UInt64 work = timestamp - regionInfo[i].start;
		
		/*
		if (cp - oldcp > work) {
			fprintf(stderr, "oldCP = 0x%llx, cp = 0x%llx, work = 0x%llx\n", oldcp, cp, work);
		}
		*/

		assert(work >= cp);

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
    if (!isPyrprofOn())
        return NULL;

    MSG(1, "binOpConst ts[%u] = ts[%u] + %u\n", dest, src, opCost);
    addWork(opCost);

#ifndef WORK_ONLY
    assert(funcHead->table != NULL); // TODO: is this /really/ necessary here?

    TEntry* entry0 = getLTEntry(src);
    TEntry* entryDest = getLTEntry(dest);
    
    //assert(entry0 != NULL);
    //assert(entryDest != NULL);

    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
        return NULL;
    
    return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
    MSG(1, "logAssignmentConst ts[%u]\n", dest);

#ifndef WORK_ONLY
    TEntry* entryDest = getLTEntry(dest);
    
    //assert(entryDest != NULL);

    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
        return NULL;

    MSG(1, "load ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOAD_COST);
    addWork(LOAD_COST);
    addLoad();

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
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
        return NULL;

    MSG(1, "store ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);

    addWork(STORE_COST);
    addStore();

#ifndef WORK_ONLY
    TEntry* entry0 = getLTEntry(src);
    TEntry* entryDest = getGTEntry(dest_addr);

#ifdef EXTRA_STATS
    TEntry* entryLine = getGTEntryCacheLine(dest_addr);
#endif

    assert(entryDest != NULL);
    assert(entry0 != NULL);

    //fprintf(stderr, "\n\nstore ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
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
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
        return;
    
    MSG(1, "logMalloc addr=0x%x size=%llu\n", addr, (UInt64)size);

#ifndef WORK_ONLY
    assert(size != 0);

    UInt32 start_index, end_index;

    start_index = ((UInt64) addr >> 16) & 0xffff;

    UInt64 end_addr = (UInt64)addr + (size-1);

    end_index = (end_addr >> 16) & 0xffff;

    assert(start_index < 0x10000 && end_index < 0x10000);

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

	addWork(MALLOC_COST);

	// update timestamp and CP
    TEntry* entryDest = getLTEntry(dest);
    
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

    int i;
    for (i = minLevel; i < maxLevel; i++) {
        UInt version = getVersion(i);
        UInt64 value = getCdt(i,version) + MALLOC_COST;

        updateTimestamp(entryDest, i, version, value);
        updateCP(value, i);
    }
#endif
}

void logFree(Addr addr) {
    if (!isPyrprofOn())
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
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
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


void addControlDep(UInt cond) {
    MSG(2, "push ControlDep ts[%u]\n", cond);

#ifndef WORK_ONLY
    CDT* toAdd = allocCDT();
    if (isPyrprofOn()) {
        TEntry* entry = getLTEntry(cond);
        fillCDT(toAdd, entry);
    }
    toAdd->next = cdtHead;
    cdtHead = toAdd;
#endif
}

void removeControlDep() {
    MSG(2, "pop  ControlDep\n");

#ifndef WORK_ONLY
    CDT* toRemove = cdtHead;
    cdtHead = cdtHead->next;
    freeCDT(toRemove);
#endif
}


// prepare timestamp storage for return value
void addReturnValueLink(UInt dest) {
    if (!isPyrprofOn())
        return;
    MSG(1, "prepare return storage ts[%u]\n", dest);
#ifndef WORK_ONLY
    funcHead->ret = getLTEntry(dest);
#endif
}

// write timestamp to the prepared storage
void logFuncReturn(UInt src) {
    if (!isPyrprofOn())
        return;
    MSG(1, "write return value ts[%u]\n", src);

#ifndef WORK_ONLY
    TEntry* srcEntry = getLTEntry(src);
    assert(funcHead->next != NULL);
    assert(funcHead->next->ret != NULL);

    // Copy the return timestamp into the previous stack's return value.
    copyTEntry(funcHead->next->ret, srcEntry);
#endif
}

void logFuncReturnConst(void) {
    if (!isPyrprofOn())
        return;
    MSG(1, "logFuncReturnConst\n");

#ifndef WORK_ONLY
    int i;
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());
    for (i = minLevel; i < maxLevel; i++) {
        int version = getVersion(i);
        UInt64 cdt = getCdt(i,version);

        // Copy the return timestamp into the previous stack's return value.
        updateTimestamp(funcHead->next->ret, i, version, cdt);
//      funcHead->ret->version[i] = version;
//      funcHead->ret->time[i] = cdt;
    }
#endif
}

void logBBVisit(UInt bb_id) {
    if (!isPyrprofOn())
        return;
#ifdef MANAGE_BB_INFO
    MSG(1, "logBBVisit(%u)\n", bb_id);
    __prevBB = __currentBB;
    __currentBB = bb_id;
#endif
}

void* logPhiNode1CD(UInt dest, UInt src, UInt cd) {
    if (!isPyrprofOn())
        return NULL;
    MSG(1, "logPhiNode1CD ts[%u] = max(ts[%u], ts[%u])\n", dest, src, cd);

#ifndef WORK_ONLY
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD = getLTEntry(cd);
    TEntry* entryDest = getLTEntry(dest);

    assert(entrySrc != NULL);
    assert(entryCD != NULL);
    assert(entryDest != NULL);
    
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
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
    
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
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
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
        return NULL;

    MSG(1, "logPhiNode4CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
    TEntry* entrySrc = getLTEntry(src);
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryCD3 = getLTEntry(cd3);
    TEntry* entryCD4 = getLTEntry(cd4);
    TEntry* entryDest = getLTEntry(dest);

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
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
        return NULL;

    MSG(1, "log4CDToPhiNode ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, dest, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
    TEntry* entryCD1 = getLTEntry(cd1);
    TEntry* entryCD2 = getLTEntry(cd2);
    TEntry* entryCD3 = getLTEntry(cd3);
    TEntry* entryCD4 = getLTEntry(cd4);
    TEntry* entryDest = getLTEntry(dest);

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
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
        return;
    MSG(1, "logPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest, src, dest);

#ifndef WORK_ONLY
    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());
    int i = 0;
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
    if (!isPyrprofOn())
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

    int minLevel = _minRegionToLog;
    int maxLevel = MIN(_maxRegionToLog+1, getLevelNum());

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
    if (!isPyrprofOn())
        return NULL;
    return logAssignmentConst(dest);
}

UInt isCpp = FALSE;
UInt hasInitialized = 0;

int pyrprofInit() {
    if(hasInitialized++) {
        MSG(0, "pyrprofInit skipped\n");
        return FALSE;
    }
    MSG(0, "pyrprofInit running\n");

    pyrprofOn = TRUE;

    fprintf(stderr,"DEBUGLEVEL = %d\n", DEBUGLEVEL);

#ifdef USE_UREGION
    initializeUdr();
#else
	cregionInit();
#endif
    levelNum = 0;
    invokeStackTop = invokeStack;
    int storageSize = _maxRegionToLog - _minRegionToLog + 1;
    MSG(0, "minLevel = %d maxLevel = %d storageSize = %d\n", 
        _minRegionToLog, _maxRegionToLog, storageSize);

    // Allocate a memory allocator.
    MemMapAllocatorCreate(&memPool, ALLOCATOR_SIZE);

    initDataStructure(storageSize);

    assert(versions = (int*) malloc(sizeof(int) * _MAX_REGION_LEVEL));
    bzero(versions, sizeof(int) * _MAX_REGION_LEVEL);

    assert(regionInfo = (Region*) malloc(sizeof(Region) * _MAX_REGION_LEVEL));
    bzero(regionInfo, sizeof(Region) * _MAX_REGION_LEVEL);

    // Allocate a deque to hold timestamps of args.
    deque_create(&argTimestamps, NULL, NULL);

    // Allocate the hash map to store dynamic region id counts.
    hash_map_sid_did_create(&sidToDid, sidHash, sidCompare, NULL, NULL);

    allocDummyTEntry();

    prepareCall(0, 0);
    cdtHead = allocCDT();
	pushFuncContext();
    
	turnOnProfiler();
    return TRUE;
}

int pyrprofDeinit() {
    if(--hasInitialized) {
        MSG(0, "pyrprofDeinit skipped\n");
        return FALSE;
    }
    MSG(0, "pyrprofDeinit running\n");

#ifdef USE_UREGION
    finalizeUdr();
#else
	cregionFinish("kremlin.bin");
#endif

    finalizeDataStructure();

    // Deallocate the arg timestamp deque.
    deque_delete(&argTimestamps);

    // Deallocate the static id to dynamic id map.
    hash_map_sid_did_delete(&sidToDid);

    freeDummyTEntry();

    free(regionInfo);
    regionInfo = NULL;

    free(versions);
    versions = NULL;
	popFuncContext();

    // Deallocate the memory allocator.
    MemMapAllocatorDelete(&memPool);

    freeCDT(cdtHead);
    cdtHead = NULL;

    fprintf(stderr, "[pyrprof] minRegionLevel = %d maxRegionLevel = %d\n", 
        _minRegionToLog, _maxRegionToLog);
    fprintf(stderr, "[pyrprof] app MaxRegionLevel = %d\n", _maxRegionNum);

    pyrprofOn = FALSE;

    return TRUE;
}

void initProfiler() {
    pyrprofInit();
}

void cppEntry() {
    isCpp = TRUE;
    pyrprofInit();
}

void cppExit() {
    pyrprofDeinit();
}

void deinitProfiler() {
	turnOffProfiler();
    pyrprofDeinit();
}

UInt64 sidHash(UInt64 sid) {
    return sid;
}

int sidCompare(UInt64 s1, UInt64 s2) {
    return s1 == s2;
}

void printProfileData() {}

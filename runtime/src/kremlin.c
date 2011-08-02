#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
//#include "kremlin.h"
#include "debug.h"
#include "kremlin_deque.h"
#include "cregion.h"
#include "hash_map.h"
#include "Vector.h"
#include "RShadow.h"
#include "MShadow.h"

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)
#define DS_ALLOC_SIZE   100     // used for static data structures
#define MAX_SRC_TSA_VAL	6

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))

#define isKremlinOn()		(kremlinOn == 1)


UInt64              loadCnt = 0llu;
UInt64              storeCnt = 0llu;
UInt64              lastCallSiteId;


UInt64 _regionFuncCnt;
UInt64 _setupTableCnt;
int _requireSetupTable;

/*****************************************************************
 * Region Level Management 
 *****************************************************************/


// min and max level for instrumentation
Level __kremlin_min_level = 0;
Level __kremlin_max_level = 21;	 

//void logLoopIteration() {}

static Level levelNum = -1;

static inline Level getCurrentLevel() {
	return levelNum;
}


inline void setMinLevel(Level level) {
	__kremlin_min_level = level;	
}

inline void setMaxLevel(Level level) {
	__kremlin_max_level = level;	
}

// what are lowest and highest levels to instrument now?
static inline Level getStartLevel() {
	return getMinLevel();
}

static inline Level getEndLevel() {
    return MIN(getMaxLevel(), getCurrentLevel());
}


static Bool _instrumentable = TRUE;

static inline Bool isInstrumentable() {
	return _instrumentable;
}

static inline Bool isLevelInstrumentable(Level level) {
	if (level >= getMinLevel() && level <= getMaxLevel())
		return TRUE;
	else 
		return FALSE;
}

static inline void updateInstrumentable(Level level) {
	_instrumentable = isLevelInstrumentable(level);
}

static inline void incrementRegionLevel() {
    levelNum++;
	updateInstrumentable(getCurrentLevel());
}

static inline void decrementRegionLevel() {
	levelNum--; 
	updateInstrumentable(getCurrentLevel());
}


/*************************************************************
 * Index Management
 * Index represents the offset in multi-value shadow memory
 *************************************************************/

static inline Level getIndex(Level level) {
	return level - getMinLevel();
}




/*************************************************************
 * Global Timestamp Management
 *************************************************************/

static Timestamp	timetick = 0llu;

inline void addWork(int work) {
	timetick += work;
}

inline Timestamp getTimetick() {
	return timetick;
}

static inline void addLoad() {
	loadCnt++;
}

static inline void addStore() {
	storeCnt++; 
}


/*************************************************************
 * Arg Management
 *
 * ToDo: use Arg Pool rather than dynamically 
 * allocate new Args 
 *************************************************************/
static deque* argQueue;
#define DUMMY_REG	-1

static inline Arg* createArg() {
	Arg* ret = malloc(sizeof(Arg));
	ret->values = malloc(sizeof(Timestamp) * getIndexSize());
	return ret;
}

static inline void freeArg(Arg* arg) {
	free(arg->values);
	free(arg);
}

static inline void putArgTimestamp(Reg src) {
	Arg* arg = createArg();
	if (src != DUMMY_REG) 
		RShadowShadowToArg(arg, src);
	else
		arg->reg = DUMMY_REG;

	deque_push_back(argQueue, arg);
}

static inline Arg* getArgTimestamp() {
	return deque_pop_front(argQueue);
}

static inline void clearArgs() {
	deque_clear(argQueue);
}



/*****************************************************************
 * Region Info Management
 *****************************************************************/

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


Region* regionInfo = NULL;

static void regionInfoInit() {
    regionInfo = (Region*) calloc(sizeof(Region), getIndexSize());
    assert(regionInfo);
}


static void regionInfoDeinit() {
    free(regionInfo);
    regionInfo = NULL;
}

static inline void regionInfoUpdateCp(Region* region, Timestamp value) {
	region->cp = MAX(value, region->cp);
}

static inline Region* getRegion(Level level) {
	return &regionInfo[level];
}

static inline void regionInfoRestart(Region* region, SID sid, DID did, UInt regionType, Level level) {
	region->regionId = sid;
	region->start = getTimetick();
	region->did = did;
	region->cp = 0LL;
	region->childrenWork = 0LL;
	region->childrenCP = 0LL;
	region->regionType = regionType;
	region->loadCnt = 0LL;
	region->storeCnt = 0LL;
#ifdef EXTRA_STATS
    region->readCnt = 0LL;
    region->writeCnt = 0LL;
    region->readLineCnt = 0LL;
    region->writeLineCnt = 0LL;
#endif
	
}

/*****************************************************************
 * Profile Control Functions
 *****************************************************************/

static Bool kremlinOn = 0;

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
    logRegionEntry(0, RegionLoop); // root region (SID, Type) = (0, Loop)
	fprintf(stderr, "[kremlin] Logging started.\n");
}

/**
 * end profiling
 *
 * pop the root region pushed in turnOnProfiler()
 */
void turnOffProfiler() {
    logRegionExit(0, RegionLoop);
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



/*****************************************************************
 * Dynamic Region Id Management
 *
 * Opt Priority: low (only called in logRegionEntry)
 ****************************************************************/

/*  HASH and VECTOR Library Declarations */
HASH_MAP_DEFINE_PROTOTYPES(sid_did, UInt64, UInt64);
HASH_MAP_DEFINE_FUNCTIONS(sid_did, UInt64, UInt64);


static hash_map_sid_did*   sidToDid;

static UInt64 sidHash(SID sid) {
    return sid;
}

static int sidCompare(SID s1, SID s2) {
    return s1 == s2;
}

static void didInit() {
    hash_map_sid_did_create(&sidToDid, sidHash, sidCompare, NULL, NULL);
}

static void didDeinit() {
    hash_map_sid_did_delete(&sidToDid);
}

/**
 * Returns a pointer to the dynamic id count.
 *
 * @param sid       The static id.
 * @return          The dynamic id count.
 */
inline DID didGet(SID sid) {
    UInt64* did;
    if(!(did = hash_map_sid_did_get(sidToDid, sid))) {
        hash_map_sid_did_put(sidToDid, sid, 0, TRUE);
        did = hash_map_sid_did_get(sidToDid, sid);
    }
    assert(did);
    return *did;
}

/**
 * Increments the dynamic id count for a static region.
 *
 * @param sid       The static id.
 */
inline void didNext(SID sid) {
	DID* did;
    if(!(did = hash_map_sid_did_get(sidToDid, sid))) {
        hash_map_sid_did_put(sidToDid, sid, 0, TRUE);
        did = hash_map_sid_did_get(sidToDid, sid);
    }
    assert(did);

    (*did)++;
}

/*****************************************************************
 * Version Management
 *
 * Opt priority: versionGet should be quick
 ****************************************************************/

// TODO: versions shouldn't be statically allocated to
// DS_ALLOC_SIZE... this should be dynamic
// Can we use getIndexSize() instead?

static Version*  versions = NULL;

void versionInit() {
    versions = (int*) calloc(sizeof(int), DS_ALLOC_SIZE);
    assert(versions);
}

void versionDeinit() {
	free(versions);
	versions = NULL;
}

Version versionGet(Level level) {
	assert(level >= 0 && level < DS_ALLOC_SIZE);
	return versions[level];
}

void versionNext(Level level) {
	versions[level]++;
}


/*****************************************************************
 * CDT Management
 *
 * getCdt: get cdt for a specific level
 * getCdt: get cdt for the current level
 * setCdt: set cdt for a specific level and version
 * fillCdt: copy timestamp from TEntry to CDT
 * 
 * 
 *****************************************************************/

typedef struct _CDT_T {
	UInt64* time;
	UInt32 size;
} CDT;


CDT* cdtHead = NULL;

/**
 * Returns timestamp of control dep at specified level with specified version.
 * @param level 	Level at which to look for control dep.
 * @param version	Version we are looking for.
 * @return			Timestamp of control dep.
 */
// preconditions: cdtHead != NULL && level >= 0
static inline Timestamp getCdt(Index index) {
    assert(cdtHead != NULL);
    //return (cdtHead->version[index] == version) ? cdtHead->time[index] : 0;
	return cdtHead->time[index];
}

/**
 * Sets the control dep at specified level to specified (version,time) pair
 * @param level		Level to set.
 * @param version	Version number to set.
 * @param time		Timestamp to set control dep to.
 */
static inline void setCdt(Index index, Timestamp time) {
	assert(index >= 0);
    cdtHead->time[index] = time;
    //cdtHead->version[index] = version;
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
    		cdtPool[i].time = (UInt64*) calloc(getIndexSize(), sizeof(UInt64));
			cdtPool[i].size = getIndexSize();
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
    	cdtPool[i].time = (UInt64*) calloc(getIndexSize(), sizeof(UInt64));
		cdtPool[i].size = getIndexSize();

		//assert(cdtPool[i].time && cdtPool[i].version);
		//fprintf(stderr,"cdtPool[%d].time = %p\n",i,cdtPool[i].time);
		//fprintf(stderr,"cdtPool[%d].version = %p\n",i,cdtPool[i].version);
	}
	cdtHead = allocCDT();
}

void deinitCDT() {
	int i=0;
	for (i=0; i<cdtSize; i++) {
		free(cdtPool[i].time);
	}
	cdtHead = NULL;
}

void addControlDep(UInt cond) {
    MSG(2, "push ControlDep ts[%u]\n", cond);

    if (!isKremlinOn()) {
		return;
	}
#ifndef WORK_ONLY
    if(isInstrumentable()) { 
		cdtHead = allocCDT(); 
	}
#endif
	Index index;
	for (index=0; index<getIndexSize(); index++) {
        cdtHead->time[index] = RShadowGetTimestamp(cond, index);
	}
}

void removeControlDep() {
    MSG(2, "pop  ControlDep\n");
#ifndef WORK_ONLY
    if(isInstrumentable()) { cdtHead = freeCDT(cdtHead); }
#endif
}


/*****************************************************************
 * Function Context Management
 * opt priority: low
 *****************************************************************/

typedef struct _FuncContext {
	LTable* table;
	Timestamp* ret;
	UInt64 callSiteId;
} FuncContext;


// A vector used to represent the call stack.
VECTOR_DEFINE_PROTOTYPES(FuncContexts, FuncContext*);
VECTOR_DEFINE_FUNCTIONS(FuncContexts, FuncContext*, VECTOR_COPY, VECTOR_NO_DELETE);

static FuncContexts*       funcContexts;

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

	//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);
}


/**
 * Removes context at the top of the function context stack.
 */
void popFuncContext() {
    FuncContext* ret = FuncContextsPopVal(funcContexts);
    assert(ret);
    //assert(ret->table != NULL);

    assert(_regionFuncCnt == _setupTableCnt);
    assert(_requireSetupTable == 0);

    if (ret->table != NULL)
        RShadowFreeTable(ret->table);

    free(ret);  
}


/*****************************************************************
 * Function Call Management
 *****************************************************************/


void prepareCall(UInt64 callSiteId, UInt64 calledRegionId) {
    if(!isKremlinOn()) { return; }

    // Clear off any argument timestamps that have been left here before the
    // call. These are left on the deque because library calls never take
    // theirs off. 
    clearArgs();
	lastCallSiteId = callSiteId;
}

// TODO: need to think how to pass args without TEntry
void linkArgToLocal(Reg src) {
    if (!isKremlinOn())
        return;
    MSG(1, "linkArgToLocal to ts[%u]\n", src);
	putArgTimestamp(src);
}

static TEntry*	dummyEntry = NULL;
static void 	allocDummyTEntry() { dummyEntry = TEntryAlloc(getIndexSize()); }
static TEntry*	getDummyTEntry() { return dummyEntry; }
static void		freeDummyTEntry() {
   free(dummyEntry); // dummy will always have NULL for time/version
   dummyEntry = NULL;
}

#define DUMMY_ARG		-1

// special case for constant arg
void linkArgToConst() {
    if (!isKremlinOn())
        return;

    MSG(1, "linkArgToConst\n");
	putArgTimestamp(DUMMY_ARG); // dummy arg
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(Reg dest) {
    if (!isKremlinOn())
        return;

    MSG(1, "transfer arg data to ts[%u]\n", dest);
	Arg* arg = getArgTimestamp();
	if (arg->reg != DUMMY_ARG)
		RShadowArgToShadow(dest, arg);
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

    LTable* table = RShadowCreateTable(maxVregNum, getIndexSize());
    FuncContext* funcHead = *FuncContextsLast(funcContexts);
    assert(funcHead->table == NULL);
    funcHead->table = table;
    assert(funcHead->table != NULL);

    RShadowSetActiveLTable(funcHead->table);
    _setupTableCnt++;
    _requireSetupTable = 0;
#endif
}


/*****************************************************************
 * logRegionEntry / logRegionExit
 *****************************************************************/

void logRegionEntry(SID regionId, RegionType regionType) {
    if (!isKremlinOn()) { return; }

    if(regionType == RegionFunc)
    {
		_regionFuncCnt++;
        pushFuncContext();
        _requireSetupTable = 1;
    }

    incrementRegionLevel();
    Level level = getCurrentLevel();

    FuncContext* funcHead = *FuncContextsLast(funcContexts);
	UInt64 callSiteId = (funcHead == NULL) ? 0x0 : funcHead->callSiteId;
	cregionPutContext(regionId, callSiteId);
    didNext(regionId);
    versionNext(level);

	 MSG(0, "[+++] region [%u, %d, %llu:%llu] start: %llu\n",
        regionType, level, regionId, didGet(regionId), getTimetick());
    incIndentTab(); // only affects debug printing


	// If we exceed the maximum depth, we act like this region doesn't exist
	if (!isInstrumentable()) {
		MSG(0, "skip region level %d as we instrument [%d, %d]\n", level, getMinLevel(), getMaxLevel());
		 return; 
	}

   	Region* region = getRegion(level);
	regionInfoRestart(region, regionId, didGet(regionId),regionType, level);

#ifndef WORK_ONLY
    setCdt(getIndex(level), 0);
#endif
}

/**
 * Does the clean up work when exiting a function region.
 */
void handleFuncRegionExit() {
	popFuncContext();

	FuncContext* funcHead = *FuncContextsLast(funcContexts);

	if (funcHead == NULL) { assert(getCurrentLevel() == 0); }
	else { RShadowSetActiveLTable(funcHead->table); }

#if MANAGE_BB_INFO
	MSG(1, "    currentBB: %u   lastBB: %u\n",
		__currentBB, __prevBB);
#endif
}


/**
 * Creates RegionField and fills it based on inputs.
 */
RegionField fillRegionField(UInt64 work, UInt64 cp, UInt64 callSiteId, UInt64 spWork, UInt64 tpWork, Region* region_info) {
	RegionField field;

    field.work = work;
    field.cp = cp;
	field.callSite = callSiteId;
	field.spWork = spWork;
	field.tpWork = tpWork;

    field.loadCnt = region_info->loadCnt;
    field.storeCnt = region_info->storeCnt;
#ifdef EXTRA_STATS
    field.readCnt = region_info->readCnt;
    field.writeCnt = region_info->writeCnt;
    field.readLineCnt = region_info->readLineCnt;
    field.writeLineCnt = region_info->writeLineCnt;
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

void logRegionExit(SID regionId, RegionType regionType) {
    if (!isKremlinOn()) { return; }

    Level level = getCurrentLevel();
	Region* region = getRegion(level);
    SID sid = regionId;
    DID did = region->did;
	SID parentSid = 0;
	DID parentDid = 0;
    UInt64 work = getTimetick() - region->start;

    decIndentTab(); // applies only to debug printing
    MSG(0, "[---] region [%u, %u, %llu:%llu] cp %llu work %llu\n",
        regionType, level, regionId, did, region->cp, work);

	// If we are outside range of levels, 
	// handle function stack then exit
	if (!isInstrumentable()) {
#ifndef WORK_ONLY
		if (regionType == RegionFunc) {
			 handleFuncRegionExit(); 
		}
#endif
    	decrementRegionLevel();
    	decIndentTab(); // applies only to debug printing
		cregionRemoveContext(NULL);
		return;
	}
    if (region->regionId != regionId) {
		fprintf(stderr, "mismatch in regionID. expected %llu, got %llu. level = %d\n", 
				region->regionId, regionId, level);
		assert(0);
	}

    UInt64 cp = region->cp;
	assert(work >= cp);
	assert(work >= region->childrenWork);

	// Only update parent region's childrenWork and childrenCP 
	// when we are logging the parent
	// If level is higher than max,
	// it will not reach here - 
	// so no need to compare with max level.

	if (level > getMinLevel()) {
		Region* parent_region = getRegion(level - 1);
    	parentSid = parent_region->regionId;
		parentDid = parent_region->did;
		parent_region->childrenWork += work;
		parent_region->childrenCP += cp;
	} 

	// Check that cp is positive if work is positive.
	// This only applies when the current level gets instrumented (otherwise this condition always holds)
    if (cp == 0 && work > 0) {
        fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
        fprintf(stderr, "region [type: %u, level: %u, id: %llu:%llu] parent [%llu:%llu] cp %llu work %llu\n",
            regionType, level, regionId, did, parentSid, parentDid, 
            region->cp, work);
        assert(0);
    }

	double spTemp = (work - region->childrenWork + region->childrenCP) / (double)cp;
	double sp = (work > 0) ? spTemp : 1.0;

	if(sp < 1.0) {
		fprintf(stderr, "sid=%lld work=%llu childrenWork = %llu childrenCP=%lld\n", sid, work,
			region->childrenWork, region->childrenCP);
		assert(0);
	}

	UInt64 spWork = (UInt64)((double)work / sp);
	UInt64 tpWork = cp;

	// due to floating point variables,
	// spWork or tpWork can be larger than work
	if (spWork > work) { spWork = work; }
	if (tpWork > work) { tpWork = work; }


    /*
    if (regionType < RegionLoopBody)
        MSG(0, "[---] region [%u, %u, %llu:%llu] parent [%llu:%llu] cp %llu work %llu\n",
                regionType, level, regionId, did, parentSid, parentDid, 
                region.cp, work);
    */

    RegionField field = fillRegionField(work, cp, (*FuncContextsLast(funcContexts))->callSiteId, 
						spWork, tpWork, region);
	cregionRemoveContext(&field);
        
#ifndef WORK_ONLY
    if (regionType == RegionFunc) { handleFuncRegionExit(); }
#endif

    decrementRegionLevel();
}


/*****************************************************************
 * logReduction / logBinary
 *****************************************************************/


void* logReductionVar(UInt opCost, Reg dest) {
    addWork(opCost);
    return NULL;
}

void* logBinaryOp(UInt opCost, Reg src0, Reg src1, Reg dest) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "binOp ts[%u] = max(ts[%u], ts[%u]) + %u\n", dest, src0, src1, opCost);
    addWork(opCost);

	if (!isInstrumentable()) 
		return NULL;

#ifndef WORK_ONLY
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

    Level i;
    for (i = minLevel; i <= maxLevel; ++i) {
		// CDT and shadow memory are index based
		Region* region = getRegion(i);
		Index index = getIndex(i);
        Timestamp cdt = getCdt(index);
        Timestamp ts0 = RShadowGetTimestamp(src0, index);
        Timestamp ts1 = RShadowGetTimestamp(src1, index);
        Timestamp greater0 = (ts0 > ts1) ? ts0 : ts1;
        Timestamp greater1 = (cdt > greater0) ? cdt : greater0;
        Timestamp value = greater1 + opCost;
		RShadowSetTimestamp(value, dest, index);

		// region info is level based
        regionInfoUpdateCp(region, value);
		
        MSG(2, "binOp[%u] level %u version %u \n", opCost, i, versionGet(i));
        MSG(2, " src0 %u src1 %u dest %u\n", src0, src1, dest);
        MSG(2, " ts0 %u ts1 %u cdt %u value %u\n", ts0, ts1, cdt, value);
    }
#endif
	return NULL;
}

void* logBinaryOpConst(UInt opCost, Reg src, Reg dest) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "binOpConst ts[%u] = ts[%u] + %u\n", dest, src, opCost);
    addWork(opCost);

#ifndef WORK_ONLY
	if (!isInstrumentable()) 
		return NULL;

    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

    Level i;
    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = getRegion(i);
		Index index = getIndex(i);
        Timestamp cdt = getCdt(index);
        Timestamp ts0 = RShadowGetTimestamp(src, index);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + opCost;
		RShadowSetTimestamp(value, dest, index);

		regionInfoUpdateCp(region, value);

        MSG(2, "binOpConst[%u] level %u version %u \n", opCost, i, versionGet(i));
        MSG(2, " src %u dest %u\n", src, dest);
        MSG(2, " ts0 %u cdt %u value %u\n", ts0, cdt, value);
    }

//    return entryDest;
    return NULL;
#else
    return NULL;
#endif
}


void* logAssignment(Reg src, Reg dest) {
    if (!isKremlinOn()) return NULL;
    
    return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "logAssignmentConst ts[%u]\n", dest);

#ifndef WORK_ONLY
	if (!isInstrumentable())
		return NULL;
    
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

    Level i;
    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = getRegion(i);
		Index index = getIndex(i);
        Timestamp cdt = getCdt(index);
		RShadowSetTimestamp(cdt, dest, index);
        regionInfoUpdateCp(region, cdt);
    }
#endif
    return NULL;
}

void* logLoadInst(Addr src_addr, Reg dest) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "load ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOAD_COST);
    addWork(LOAD_COST);

#ifndef WORK_ONLY
	if (!isInstrumentable())
		return NULL;
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();
    Level i;
    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = getRegion(i);
		Index index = getIndex(i);
        region->loadCnt++;
        Timestamp cdt = getCdt(index);
		Timestamp ts0 = MShadowGetTimestamp(src_addr, index);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + LOAD_COST;

#ifdef EXTRA_STATS
        //updateReadMemoryAccess(entry0, i, versionGet(i), value);
#endif

        RShadowSetTimestamp(value, dest, index);
        regionInfoUpdateCp(region, value);
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
	if (!isInstrumentable()) return NULL;


    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

    Level i;

    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = getRegion(i);
		Index index = getIndex(i);
        Timestamp cdt = getCdt(index);
		Timestamp ts_src_addr = MShadowGetTimestamp(src1, index);
		Timestamp ts_src1 = RShadowGetTimestamp(src1, index);

        Timestamp max1 = (ts_src_addr > cdt) ? ts_src_addr : cdt;
        Timestamp max2 = (max1 > ts_src1) ? max1 : ts_src1;
		Timestamp value = max2 + LOAD_COST;

        RShadowSetTimestamp(value, dest, index);
        regionInfoUpdateCp(region, value);

        MSG(2, "logLoadInst1Src level %u version %u \n", i, versionGet(i));
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
	if(!isInstrumentable()) return NULL;

    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();
    Level i;

    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = getRegion(i);
		Index index = getIndex(i);
        region->storeCnt++;
        Timestamp cdt = getCdt(index);
		Timestamp ts0 = RShadowGetTimestamp(src, index);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + STORE_COST;
#ifdef EXTRA_STATS
        //updateWriteMemoryAccess(entryDest, i, versionGet(i), value);
#endif
		MShadowSetTimestamp(value, dest_addr, index);
        regionInfoUpdateCp(region, value);
    }

#endif
    //return entryDest;
    return NULL;
}


void* logStoreInstConst(Addr dest_addr) {
    if (!isKremlinOn())
        return NULL;

    MSG(1, "storeConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    addWork(STORE_COST);

#ifndef WORK_ONLY
	if (!isInstrumentable()) return NULL;

#ifdef EXTRA_STATS
    //TEntry* entryLine = getGTEntryCacheLine(dest_addr);
#endif
    
    //fprintf(stderr, "\nstoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();
    Level i;
    for (i = minLevel; i <= maxLevel; ++i) {
		Region* region = getRegion(i);
		Index index = getIndex(i);
        Timestamp cdt = getCdt(index);
        Timestamp value = cdt + STORE_COST;
#ifdef EXTRA_STATS
        //updateWriteMemoryAccess(entryDest, i, version, value);
        //updateWriteMemoryLineAccess(entryLine, i, version, value);
#endif
		MShadowSetTimestamp(value, dest_addr, index);
		regionInfoUpdateCp(region, value);
    }
#endif
    return NULL;
}


// prepare timestamp storage for return value
void addReturnValueLink(Reg dest) {
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
void logFuncReturn(Reg src) {
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
    TEntryCopy((*nextHead)->ret, srcEntry);
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

    TEntryRealloc((*nextHead)->ret, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
		Index index = getIndex(i);
        int version = versionGet(i);
        UInt64 cdt = getCdt(index);

        // Copy the return timestamp into the previous stack's return value.
        //RShadowSetTimestamp((*nextHead)->ret, i, version, cdt);
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

// this function is the same as logAssignmentConst but helps to quickly
// identify induction variables in the source code
void* logInductionVar(UInt dest) {
    if (!isKremlinOn()) return NULL;
    return logAssignmentConst(dest);
}

/******************************************************************
 * logPhi Functions
 *
 *  for the efficiency, we use several versions with different 
 *  number of incoming dependences
 ******************************************************************/

void* logPhiNode1CD(UInt dest, UInt src, UInt cd) {
    if (!isKremlinOn()) return NULL;

    MSG(1, "logPhiNode1CD ts[%u] = max(ts[%u], ts[%u])\n", dest, src, cd);

#ifndef WORK_ONLY
	if(!isInstrumentable()) return NULL;
    
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts_src = RShadowGetTimestamp(src, i);
		Timestamp ts_cd = RShadowGetTimestamp(cd, i);
        Timestamp max = (ts_src > ts_cd) ? ts_src : ts_cd;
        //updateTimestamp(entryDest, i, version, max);
		RShadowSetTimestamp(max, dest, i);
        MSG(2, "logPhiNode1CD level %u version %u \n", i, versionGet(i));
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
	if(!isInstrumentable()) return NULL;
    
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts_src = RShadowGetTimestamp(src, i);
		Timestamp ts_cd1 = RShadowGetTimestamp(cd1, i);
		Timestamp ts_cd2 = RShadowGetTimestamp(cd2, i);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;

		RShadowSetTimestamp(max2, dest, i);

        MSG(2, "logPhiNode2CD level %u version %u \n", i, versionGet(i));
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
	if(!isInstrumentable()) return NULL;

    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts_src = RShadowGetTimestamp(src, i);
		Timestamp ts_cd1 = RShadowGetTimestamp(cd1, i);
		Timestamp ts_cd2 = RShadowGetTimestamp(cd2, i);
		Timestamp ts_cd3 = RShadowGetTimestamp(cd3, i);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;

		RShadowSetTimestamp(max3, dest, i);

        MSG(2, "logPhiNode3CD level %u version %u \n", i, versionGet(i));
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
	if(!isInstrumentable()) return NULL;

    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
		Timestamp ts_src = RShadowGetTimestamp(src, i);
		Timestamp ts_cd1 = RShadowGetTimestamp(cd1, i);
		Timestamp ts_cd2 = RShadowGetTimestamp(cd2, i);
		Timestamp ts_cd3 = RShadowGetTimestamp(cd3, i);
		Timestamp ts_cd4 = RShadowGetTimestamp(cd4, i);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Timestamp max4 = (max3 > ts_cd4) ? max3 : ts_cd4;

		RShadowSetTimestamp(max4, dest, i);

        MSG(2, "logPhiNode4CD level %u version %u \n", i, versionGet(i));
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
	if(!isInstrumentable()) return NULL;
	
    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
        Timestamp ts_dest = RShadowGetTimestamp(dest, i);
		Timestamp ts_cd1 = RShadowGetTimestamp(cd1, i);
		Timestamp ts_cd2 = RShadowGetTimestamp(cd2, i);
		Timestamp ts_cd3 = RShadowGetTimestamp(cd3, i);
		Timestamp ts_cd4 = RShadowGetTimestamp(cd4, i);
        Timestamp max1 = (ts_dest > ts_cd1) ? ts_dest : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Timestamp max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
		RShadowSetTimestamp(max4, dest, i);

        MSG(2, "log4CDToPhiNode4CD level %u version %u \n", i, versionGet(i));
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
	if(!isInstrumentable()) return NULL;

    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();
    int i = 0;

    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = getRegion(i);
		Timestamp ts0 = RShadowGetTimestamp(src, i);
		Timestamp ts1 = RShadowGetTimestamp(dest, i);
        Timestamp value = (ts0 > ts1) ? ts0 : ts1;
		RShadowSetTimestamp(value, dest, i);
        regionInfoUpdateCp(region, value);
        MSG(2, "logPhiAddCond level %u version %u \n", i, versionGet(i));
        MSG(2, " src %u dest %u\n", src, dest);
        MSG(2, " ts0 %u ts1 %u value %u\n", ts0, ts1, value);
    }
	
#endif
}

/******************************
 * Start Function Management
 *
 *****************************/

static TEntry* mainReturn;
void deinitStartFuncContext()
{
	popFuncContext();

    assert(FuncContextsEmpty(funcContexts));
    TEntryFree(mainReturn);
    mainReturn = NULL;
}

void initStartFuncContext()
{
    assert(FuncContextsEmpty(funcContexts));

    prepareCall(0, 0);
	pushFuncContext();
    (*FuncContextsLast(funcContexts))->ret = mainReturn = TEntryAlloc(getIndexSize());
}



/******************************
 * Kremlin Init / Deinit
 *****************************/

static UInt hasInitialized = 0;

static void initInternals() {
    FuncContextsCreate(&funcContexts);
	versionInit();
	regionInfoInit();
    deque_create(&argQueue, NULL, NULL);
	didInit();
	allocDummyTEntry(); // TODO: abstract this for new shadow mem imp
	initCDT();
    initStartFuncContext();
    MemMapAllocatorCreate(&memPool, ALLOCATOR_SIZE);
}

static void deinitInternals() {
    deinitStartFuncContext();
    FuncContextsDelete(&funcContexts);
	deinitCDT();
	freeDummyTEntry();
    deque_delete(&argQueue);
	didDeinit();
    versionDeinit();
	regionInfoDeinit();
    MemMapAllocatorDelete(&memPool);
}

Bool kremlinInit() {
	DebugInit("kremlin.log");
    if(hasInitialized++) {
        MSG(0, "kremlinInit skipped\n");
        return FALSE;
    }
    MSG(0, "kremlinInit running\n");


	if(getKremlinDebugFlag()) { 
		fprintf(stderr,"[kremlin] debugging enabled at level %d\n", getKremlinDebugLevel()); 
	}

#if 0
    InvokeRecordsCreate(&invokeRecords);
#endif

    MSG(0, "Profile Level = (%d, %d), Index Size = %d\n", 
        getMinLevel(), getMaxLevel(), getIndexSize());

	initInternals();
	cregionInit();
	RShadowInit(getIndexSize());
	MShadowInit();

   	turnOnProfiler();
    return TRUE;
}




Bool kremlinDeinit() {
    if(--hasInitialized) {
        MSG(0, "kremlinDeinit skipped\n");
        return FALSE;
    }
    MSG(0, "kremlinDeinit running\n");

	turnOffProfiler();

	cregionFinish("kremlin.bin");
	deinitInternals();
	RShadowFinalize();
	MShadowFinalize();
	DebugDeinit();

    return TRUE;
}

void initProfiler() {
    kremlinInit();
}

void deinitProfiler() {
    kremlinDeinit();
}

void printProfileData() {}


/**************************************************************************
 * Start of Non-Essential APIs
 *************************************************************************/

/***********************************************
 * Library Call
 * DJ: will be optimized later..
 ************************************************/


// use estimated cost for a callee function we cannot instrument
// TODO: implement new shadow mem interface
void* logLibraryCall(UInt cost, UInt dest, UInt num_in, ...) {
#if 0
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

    TEntryRealloc(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = versionGet(i);
        UInt64 max = 0;
        
		/*
        int j;
        for (j = 0; j < num_in; j++) {
            UInt64 ts = getTimetick(entrySrc[j], i, version);
            if (ts > max)
                max = ts;
        } */  
        
		UInt64 value = max + cost;

		updateCP(value, i);
    }
    return entryDest;
#else
    return NULL;
#endif
#endif
    
}

/***********************************************
 * Dynamic Memory Allocation / Deallocation
 * DJ: will be optimized later..
 ************************************************/


// FIXME: support 64 bit address
void logMalloc(Addr addr, size_t size, UInt dest) {
#if 0
    if (!isKremlinOn()) return;
    
    MSG(1, "logMalloc addr=0x%x size=%llu\n", addr, (UInt64)size);

#ifndef WORK_ONLY

    // Don't do anything if malloc returned NULL
    if(!addr) { return; }

    createMEntry(addr,size);
#endif
#endif
}

// TODO: implement for new shadow mem interface
void logFree(Addr addr) {
#if 0
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
        UInt64 value = getCdt(i) + FREE_COST;

        updateCP(value, i);
    }
#endif
#endif
}

// TODO: more efficient implementation (if old_addr = new_addr)
// XXX: This is wrong. Values in the realloc'd location should still have the
// same timestamp.
void logRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest) {
#if 0
    if (!isKremlinOn())
        return;

    MSG(1, "logRealloc old_addr=0x%x new_addr=0x%x size=%llu\n", old_addr, new_addr, (UInt64)size);
    logFree(old_addr);
    logMalloc(new_addr,size,dest);
#endif
}

/***********************************************
 * DJ: not sure what these are for 
 ************************************************/

void* logInsertValue(UInt src, UInt dest) {
    //printf("Warning: logInsertValue not correctly implemented\n");
    return logAssignment(src, dest);
}

void* logInsertValueConst(UInt dest) {
    //printf("Warning: logInsertValueConst not correctly implemented\n");
    return logAssignmentConst(dest);
}

/***********************************************
 * Kremlin CPP Support Functions (Experimental)
 * 
 * Description will be filled later
 ************************************************/

#ifdef CPP

UInt isCpp = FALSE;

void cppEntry() {
    isCpp = TRUE;
    kremlinInit();
}

void cppExit() {
    kremlinDeinit();
}

typedef struct _InvokeRecord {
	UInt64 id;
	int stackHeight;
} InvokeRecord;

InvokeRecords*      invokeRecords;
// A vector used to record invoked calls.
VECTOR_DEFINE_PROTOTYPES(InvokeRecords, InvokeRecord);
VECTOR_DEFINE_FUNCTIONS(InvokeRecords, InvokeRecord, VECTOR_COPY, VECTOR_NO_DELETE);


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

#endif 

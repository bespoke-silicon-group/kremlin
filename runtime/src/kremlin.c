#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "debug.h"
#include "kremlin_deque.h"
#include "CRegion.h"
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


static Bool kremlinOn = 0;
UInt64              loadCnt = 0llu;
UInt64              storeCnt = 0llu;


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
#if 0	
    incIndentTab(); // only affects debug printing
	MSG(1, "instrumentable = %d\n", _instrumentable);
	decIndentTab();
#endif
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

static Time	timetick = 0llu;

inline void addWork(int work) {
	timetick += work;
}

inline Time getTimetick() {
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
 * Function Arg Transfer Sequence
 * 1) caller calls "prepareCall" to reset argFifo
 * 2) for each arg, the caller calls linkArgToLocal or linkArgToConst
 * 3) callee function enters with "logEnterRegion"
 * 4) callee links each arg with transferAndUnlinkArg by 
 * dequeing a register number from fifo,
 * in the same order used in linkArgXXXX.
 *
 * FIFO size is conservatively set to 64, 
 * which is already very large for # of args for a function call.
 *
 * Is single FIFO enough in Kremlin?
 *   Yes, function args will be prepared and processed 
 *   before and after logRegionEntry.
 *   The fifo can be reused for the next function calls.
 *   
 * Why reset the queue every time?
 *   uninstrumented functions (e.g. library) do not have 
 *  "transferAndUnlinkArg" call, so there could be 
 *  remaining args from previous function calls
 *  
 * 
 *************************************************************/

#define ARG_SIZE		64
static Reg  argFifo[ARG_SIZE];
static Reg* argFifoReadPointer;
static Reg* argFifoWritePointer;

static inline void ArgFifoInit() {
	argFifoReadPointer = argFifo;
	argFifoWritePointer = argFifo;
}

static inline void ArgFifoDeinit() {
	// intentionally blank
}

static inline void ArgFifoPush(Reg src) {
	*argFifoWritePointer++ = src;
	assert(argFifoWritePointer < argFifo + ARG_SIZE);
}

static inline Reg ArgFifoPop() {
	assert(argFifoReadPointer < argFifoWritePointer);
	return *argFifoReadPointer++;
}

static inline void ArgFifoClear() {
	argFifoReadPointer = argFifo;
	argFifoWritePointer = argFifo;
}


/*****************************************************************
 * Function Context Management
 * opt priority: low
 *****************************************************************/

static CID	lastCallSiteId;

typedef struct _FuncContext {
	LTable* table;
	Reg ret;
	CID callSiteId;
	UInt32 code;
} FuncContext;

// A vector used to represent the call stack.
VECTOR_DEFINE_PROTOTYPES(FuncContexts, FuncContext*);
VECTOR_DEFINE_FUNCTIONS(FuncContexts, FuncContext*, VECTOR_COPY, VECTOR_NO_DELETE);

static FuncContexts*       funcContexts;
#define DUMMY_RET		-1

/**
 * Pushes new context onto function context stack.
 */
static void RegionPushFunc(CID cid) {
    FuncContext* funcContext = (FuncContext*) malloc(sizeof(FuncContext));
    assert(funcContext);

    FuncContextsPushVal(funcContexts, funcContext);
    funcContext->table = NULL;
	funcContext->callSiteId = cid;
	funcContext->ret = DUMMY_RET;
	funcContext->code = 0xDEADBEEF;

    MSG(1, "RegionPushFunc at 0x%x CID 0x%x\n", funcContext, cid);
	//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);
}

/**
 * Removes context at the top of the function context stack.
 */
static void RegionPopFunc() {
    FuncContext* func = FuncContextsPopVal(funcContexts);
    assert(func);
    MSG(1, "RegionPopFunc at 0x%x CID 0x%x\n", func, func->callSiteId);

    assert(_regionFuncCnt == _setupTableCnt);
    assert(_requireSetupTable == 0);

    if (func->table != NULL)
        RShadowFreeTable(func->table);

    free(func);  
	checkRegion();
}

static FuncContext* RegionGetFunc() {
    FuncContext** func = FuncContextsLast(funcContexts);
	if (func == NULL) {
    	MSG(3, "RegionGetFunc  NULL\n");
		return NULL;
	}

    MSG(3, "RegionGetFunc  0x%x CID 0x%x\n", *func, (*func)->callSiteId);
	assert((*func)->code == 0xDEADBEEF);
	return *func;
}

static FuncContext* RegionGetCallerFunc() {
    //assert(FuncContextsSize(funcContexts) > 1);
	if (FuncContextsSize(funcContexts) == 1) {
    	MSG(3, "RegionGetCallerFunc  No Caller Context\n");
		return NULL;
	}
    FuncContext** func = FuncContextsLast(funcContexts) - 1;
    MSG(3, "RegionGetCallerFunc  0x%x CID 0x%x\n", *func, (*func)->callSiteId);
	assert((*func)->code == 0xDEADBEEF);
	//assert((*func)->table != NULL);
	return *func;
}


//static TEntry* mainReturn;
static void RegionInitFunc()
{
    FuncContextsCreate(&funcContexts);
    assert(FuncContextsEmpty(funcContexts));
	//RegionPushFunc(lastCallSiteId);
	//FuncContext* func = RegionGetFunc();
    //func->ret = DUMMY_RET;
    //prepareCall(0, 0);
}

static void RegionDeinitFunc()
{
	//RegionPopFunc();
    assert(FuncContextsEmpty(funcContexts));
	MSG(0, "RegionDeinitFunc start\n");
    FuncContextsDelete(&funcContexts);
	MSG(0, "RegionDeinitFunc end\n");
}

/*****************************************************************
 * Region Management
 *****************************************************************/

typedef struct _region_t {
	UInt32 code;
	Version version;
	SID	regionId;
	RegionType regionType;
	Timestamp start;
	Timestamp cp;
	Timestamp childrenWork;
	Timestamp childrenCP;
#ifdef EXTRA_STATS
	UInt64 loadCnt;
	UInt64 storeCnt;
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 writeLineCnt;
#endif
} Region;


Region* regionInfo = NULL;

static void RegionInit() {
    regionInfo = (Region*) malloc(sizeof(Region) * getIndexSize());
	assert(regionInfo != NULL);
	int i;
	for (i=0; i<getIndexSize(); i++) {
		regionInfo[i].code = 0xDEADBEEF;
		regionInfo[i].version = 0;
	}
	checkRegion();
    assert(regionInfo);
	//RegionVersionInit();
	RegionInitFunc();

}


static void RegionDeinit() {
	RegionDeinitFunc();
	//RegionVersionDeinit();
	checkRegion();
	assert(regionInfo != NULL);
    //free(regionInfo);
    regionInfo = NULL;
}

static inline void RegionUpdateCp(Region* region, Timestamp value) {
	region->cp = MAX(value, region->cp);
}

static inline Region* RegionGet(Level level) {
	Region* ret = &regionInfo[level];
	return ret;
}

static Version RegionGetVersion(Level level) {
	return RegionGet(level)->version;
}


void checkRegion() {
	int i;
	int bug = 0;
	for (i=0; i<getIndexSize(); i++) {
		Region* ret = &regionInfo[i];
		assert(ret->code == 0xDEADBEEF);
		if (ret->cp > 1000) {
			fprintf(stderr, "problem at level %d\n", i); 
			bug = 1;
		}
	}
	if (bug > 0)
		assert(0);
}

static inline void RegionRestart(Region* region, SID sid, UInt regionType, Level level) {
	region->version++;
	region->regionId = sid;
	region->start = getTimetick();
	region->cp = 0ULL;
	region->childrenWork = 0LL;
	region->childrenCP = 0LL;
	region->regionType = regionType;
#ifdef EXTRA_STATS
	region->loadCnt = 0LL;
	region->storeCnt = 0LL;
    region->readCnt = 0LL;
    region->writeCnt = 0LL;
    region->readLineCnt = 0LL;
    region->writeLineCnt = 0LL;
#endif
	
}


/*****************************************************************
 * CDep Management
 *
 * CDepGet: get cdt for a specific level
 * CDepGet: get cdt for the current level
 * CDepSet: set cdt for a specific level and version
 * fillCdt: copy timestamp from TEntry to CDep
 * 
 * 
 *****************************************************************/

//typedef TArray CDep;
typedef struct _CDep {
	Timestamp* time;
	int size;	
} CDep;

CDep* cdepHead = NULL;

/**
 * Returns timestamp of control dep at specified level with specified version.
 * @param level 	Level at which to look for control dep.
 * @param version	Version we are looking for.
 * @return			Timestamp of control dep.
 */
static inline Timestamp CDepGet(Index index) {
    assert(cdepHead != NULL);
	return cdepHead->time[index];
}

/**
 * Sets the control dep at specified level to specified (version,time) pair
 * @param level		Level to set.
 * @param version	Version number to set.
 * @param time		Timestamp to set control dep to.
 *
 */
static inline void CDepSet(Index index, Timestamp time) {
	assert(index >= 0);
    cdepHead->time[index] = time;
}

#define CDEP_SIZE	256
static CDep* cdtPool;
static int cdtSize;
static int cdtIndex;

static CDep* CDepAlloc() {
	CDep* ret = &cdtPool[cdtIndex];
	cdtIndex++;
	if (cdtIndex == cdtSize) {
		int i;
		cdtSize += CDEP_SIZE;
		cdtPool = realloc(cdtPool, sizeof(CDep) * cdtSize);
		for (i=cdtSize-CDEP_SIZE; i<cdtSize; i++) {
    		cdtPool[i].time = (UInt64*) calloc(getIndexSize(), sizeof(UInt64));
			cdtPool[i].size = getIndexSize();
		}
	};
	return ret;
}

static CDep* CDepFree(CDep* toFree) { 
	cdtIndex--;
	return &cdtPool[cdtIndex]; 
}

void CDepInit() {
	int i=0;
	cdtPool = malloc(sizeof(CDep) * CDEP_SIZE);

	cdtIndex = 0;
	cdtSize = CDEP_SIZE;

	for (i=0; i<CDEP_SIZE; i++) {
    	cdtPool[i].time = (UInt64*) calloc(getIndexSize(), sizeof(UInt64));
		cdtPool[i].size = getIndexSize();
	}
	cdepHead = CDepAlloc();
}

void CDepDeinit() {
	int i=0;
	for (i=0; i<cdtSize; i++) {
		free(cdtPool[i].time);
	}
	cdepHead = NULL;
}



/****************************************************************
 *
 * Kremlib Compiler Interface Functions
 *
 *****************************************************************/

/*****************************************************************
 * Control Dependence Management
 *****************************************************************/

void addControlDep(Reg cond) {
    MSG(1, "push ControlDep ts[%u]\n", cond);

    if (!isKremlinOn()) {
		return;
	}
#ifndef WORK_ONLY
	cdepHead = CDepAlloc(); 
	Index index;
	for (index=0; index<getIndexSize(); index++) {
		CDepSet(index, RShadowGet(cond, index));
	}
#endif
}

void removeControlDep() {
    MSG(1, "pop  ControlDep\n");
    if (!isKremlinOn()) {
		return;
	}
#ifndef WORK_ONLY
	cdepHead = CDepFree(cdepHead); 
#endif
}

/*****************************************************************
 * Function Call Management
 *****************************************************************/


void prepareCall(CID callSiteId, UInt64 calledRegionId) {
    MSG(1, "prepareCall\n");
    if(!isKremlinOn()) { 
		return; 
	}

    // Clear off any argument timestamps that have been left here before the
    // call. These are left on the deque because library calls never take
    // theirs off. 
    ArgFifoClear();
	lastCallSiteId = callSiteId;
}

// TODO: need to think how to pass args without TEntry
void linkArgToLocal(Reg src) {
    MSG(1, "linkArgToLocal to ts[%u]\n", src);
    if (!isKremlinOn())
        return;
	ArgFifoPush(src);
}

#define DUMMY_ARG		-1

// special case for constant arg
void linkArgToConst() {
    MSG(1, "linkArgToConst\n");
    if (!isKremlinOn())
        return;

	ArgFifoPush(DUMMY_ARG); // dummy arg
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(Reg dest) {
    MSG(1, "transfer arg data to ts[%u] \n", dest);
    if (!isKremlinOn())
        return;

	Reg src = ArgFifoPop();
	// copy parent's src timestamp into the currenf function's dest reg
	if (src != DUMMY_ARG) {
		FuncContext* caller = RegionGetCallerFunc();
		FuncContext* callee = RegionGetFunc();
		int indexSize = caller->table->indexSize;
		RShadowCopy(callee->table, dest, caller->table, src, 0, caller->table->indexSize);
	}
}


/**
 * Setup the local shadow register table.
 * @param maxVregNum	Number of virtual registers to allocate.
 */
void setupLocalTable(UInt maxVregNum) {
    MSG(1, "setupLocalTable size %u \n", maxVregNum);
    if(!isKremlinOn()) {
		 return; 
	}

#ifndef WORK_ONLY
    assert(_requireSetupTable == 1);

    LTable* table = RShadowCreateTable(maxVregNum, getIndexSize());
    FuncContext* funcHead = RegionGetFunc();
	assert(funcHead != NULL);
    assert(funcHead->table == NULL);
    funcHead->table = table;
    assert(funcHead->table != NULL);

    RShadowActivateTable(funcHead->table);
    _setupTableCnt++;
    _requireSetupTable = 0;
#endif
}


/*****************************************************************
 * Profile Control Functions
 *****************************************************************/


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
    MSG(0, "turnOnProfiler\n");
	fprintf(stderr, "[kremlin] Logging started.\n");
}

/**
 * end profiling
 *
 * pop the root region pushed in turnOnProfiler()
 */
void turnOffProfiler() {
    kremlinOn = 0;
    MSG(0, "turnOffProfiler\n");
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
 * logRegionEntry / logRegionExit
 *****************************************************************/

void logRegionEntry(SID regionId, RegionType regionType) {
    if (!isKremlinOn()) { 
		return; 
	}

    incrementRegionLevel();
    Level level = getCurrentLevel();
	Region* region = RegionGet(level);
	RegionRestart(region, regionId, regionType, level);

	MSG(0, "\n");
	MSG(0, "[+++] region [type %u, level %d, sid 0x%llx] start: %llu\n",
        regionType, level, regionId, getTimetick());
    incIndentTab(); // only affects debug printing


    if(regionType == RegionFunc) {
		_regionFuncCnt++;
        RegionPushFunc(lastCallSiteId);
        _requireSetupTable = 1;
    }

    FuncContext* funcHead = RegionGetFunc();
	CID callSiteId = (funcHead == NULL) ? 0x0 : funcHead->callSiteId;
	CRegionEnter(regionId, callSiteId);


#ifndef WORK_ONLY
	if (isInstrumentable())	
    	CDepSet(getIndex(level), 0);
#endif
	MSG(0, "\n");
}

/**
 * Does the clean up work when exiting a function region.
 */
static void handleFuncRegionExit() {
	RegionPopFunc();

	// root function
	if (FuncContextsSize(funcContexts) == 0) {
		assert(getCurrentLevel() == 0); 
		return;
	}

	FuncContext* funcHead = RegionGetFunc();
	assert(funcHead != NULL);
	RShadowActivateTable(funcHead->table); 
}


/**
 * Creates RegionField and fills it based on inputs.
 */
RegionField fillRegionField(UInt64 work, UInt64 cp, CID callSiteId, UInt64 spWork, UInt64 tpWork, Region* region_info) {
	RegionField field;

    field.work = work;
    field.cp = cp;
	field.callSite = callSiteId;
	field.spWork = spWork;
	field.tpWork = tpWork;

#ifdef EXTRA_STATS
    field.loadCnt = region_info->loadCnt;
    field.storeCnt = region_info->storeCnt;
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
    if (!isKremlinOn()) { 
		return; 
	}

    Level level = getCurrentLevel();
	Region* region = RegionGet(level);
    SID sid = regionId;
	SID parentSid = 0;
    UInt64 work = getTimetick() - region->start;
	decIndentTab(); // applies only to debug printing
	MSG(0, "\n");
    MSG(0, "[---] region [%u, %u, 0x%llx] cp %llu work %llu\n",
        regionType, level, regionId, region->cp, work);


	checkRegion();

	// If we are outside range of levels, 
	// handle function stack then exit
#if 0
	if (!isInstrumentable()) {
#ifndef WORK_ONLY
		if (regionType == RegionFunc) {
			 handleFuncRegionExit(); 
		}
#endif
    	MSG(0, "Skip - level %d is out of instrumentation range (%d, %d)\n",
			level, getMinLevel(), getMaxLevel());
    	decrementRegionLevel();
		CRegionLeave(NULL);
		return;
	}
#endif

	assert(region->regionId == regionId);
#if 0
    if (region->regionId != regionId) {
		fprintf(stderr, "mismatch in regionID. expected %llu, got %llu. level = %d\n", 
				region->regionId, regionId, level);
		assert(0);
	}
#endif

    UInt64 cp = region->cp;
	if (work < cp) {
		fprintf(stderr, "work = %llu\n", work);
		fprintf(stderr, "cp = %llu\n", cp);
		checkRegion();
		assert(0);
	}
	if (level < getMaxLevel() && level >= getMinLevel()) {
		assert(work >= cp);
		assert(work >= region->childrenWork);
		assert(work < 100000);
		assert(cp < 100000);
	}

	// Only update parent region's childrenWork and childrenCP 
	// when we are logging the parent
	// If level is higher than max,
	// it will not reach here - 
	// so no need to compare with max level.

	if (level > getMinLevel()) {
		Region* parent_region = RegionGet(level - 1);
    	parentSid = parent_region->regionId;
		parent_region->childrenWork += work;
		parent_region->childrenCP += cp;
	} 

	// Check that cp is positive if work is positive.
	// This only applies when the current level gets instrumented (otherwise this condition always holds)
    if (isInstrumentable() && cp == 0 && work > 0) {
        fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
        fprintf(stderr, "region [type: %u, level: %u, sid: %llu] parent [%llu] cp %llu work %llu\n",
            regionType, level, regionId,  parentSid,  region->cp, work);
        assert(0);
    }

	double spTemp = (work - region->childrenWork + region->childrenCP) / (double)cp;
	double sp = (work > 0) ? spTemp : 1.0;

	if (level < getMaxLevel() && sp < 1.0) {
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

	CID cid = RegionGetFunc()->callSiteId;
    RegionField field = fillRegionField(work, cp, cid, 
						spWork, tpWork, region);
	CRegionLeave(&field);
        
    if (regionType == RegionFunc) { 
		handleFuncRegionExit(); 
	}

    decrementRegionLevel();

	MSG(0, "\n");
}


/*****************************************************************
 * logReduction / logBinary
 *****************************************************************/


void* logReductionVar(UInt opCost, Reg dest) {
    MSG(1, "logReductionVar ts[%u] with cost = %d\n", dest, opCost);
    if (!isKremlinOn() || !isInstrumentable())
		return;

    addWork(opCost);
    return NULL;
}

void* logBinaryOp(UInt opCost, Reg src0, Reg src1, Reg dest) {
    MSG(1, "binOp ts[%u] = max(ts[%u], ts[%u]) + %u\n", dest, src0, src1, opCost);
    if (!isKremlinOn())
        return NULL;

    addWork(opCost);

#ifndef WORK_ONLY
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

    Level i;
    for (i = minLevel; i <= maxLevel; ++i) {
		// CDep and shadow memory are index based
		Region* region = RegionGet(i);
		Index index = getIndex(i);
        Timestamp cdt = CDepGet(index);
        Timestamp ts0 = RShadowGet(src0, index);
        Timestamp ts1 = RShadowGet(src1, index);
        Timestamp greater0 = (ts0 > ts1) ? ts0 : ts1;
        Timestamp greater1 = (cdt > greater0) ? cdt : greater0;
        Timestamp value = greater1 + opCost;
		RShadowSet(value, dest, index);

		// region info is level based
        RegionUpdateCp(region, value);
		
        MSG(2, "binOp[%u] level %u version %u \n", opCost, i, RegionGetVersion(i));
        MSG(2, " src0 %u src1 %u dest %u\n", src0, src1, dest);
        MSG(2, " ts0 %u ts1 %u cdt %u value %u\n", ts0, ts1, cdt, value);
    }
#endif
	return NULL;
}

void* logBinaryOpConst(UInt opCost, Reg src, Reg dest) {
    MSG(1, "binOpConst ts[%u] = ts[%u] + %u\n", dest, src, opCost);
    if (!isKremlinOn())
        return NULL;

    addWork(opCost);

#ifndef WORK_ONLY
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

    Level i;
    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = RegionGet(i);
		Index index = getIndex(i);
        Timestamp cdt = CDepGet(index);
        Timestamp ts0 = RShadowGet(src, index);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + opCost;
		RShadowSet(value, dest, index);

		RegionUpdateCp(region, value);

        MSG(2, "binOpConst[%u] level %u version %u \n", opCost, i, RegionGetVersion(i));
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
    MSG(1, "logAssignment ts[%u] <- ts[%u]\n", dest, src);
    if (!isKremlinOn())
    	return NULL;
    
    return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
    MSG(1, "logAssignmentConst ts[%u]\n", dest);
    if (!isKremlinOn())
        return NULL;


#ifndef WORK_ONLY
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

    Level i;
    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = RegionGet(i);
		Index index = getIndex(i);
        Timestamp cdt = CDepGet(index);
		RShadowSet(cdt, dest, index);
        RegionUpdateCp(region, cdt);
    }
#endif
    return NULL;
}

void* logLoadInst(Addr src_addr, Reg dest) {
    MSG(1, "load ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOAD_COST);
    if (!isKremlinOn())
    	return NULL;

    addWork(LOAD_COST);

#ifndef WORK_ONLY
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();
    Level i;
    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = RegionGet(i);
		Index index = getIndex(i);
		Version version = RegionGetVersion(i);
        Timestamp cdt = CDepGet(index);
		Timestamp ts0 = MShadowGet(src_addr, index, version);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + LOAD_COST;

#ifdef EXTRA_STATS
        region->loadCnt++;
        //updateReadMemoryAccess(entry0, i, RegionGetVersion(i), value);
#endif
        RShadowSet(value, dest, index);
        RegionUpdateCp(region, value);
    }

    //return entryDest;
    return NULL;
#else
    return NULL;
#endif
}

void* logLoadInst1Src(Addr src_addr, UInt src1, UInt dest) {
    MSG(1, "load ts[%u] = max(ts[0x%x],ts[%u]) + %u\n", dest, src_addr, src1, LOAD_COST);
    if (!isKremlinOn())
		return NULL;

    addWork(LOAD_COST);

#ifndef WORK_ONLY
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

    Level i;

    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = RegionGet(i);
		Index index = getIndex(i);
		Version version = RegionGetVersion(i);
        Timestamp cdt = CDepGet(index);
		Timestamp ts_src_addr = MShadowGet(src1, index, version);
		Timestamp ts_src1 = RShadowGet(src1, index);

        Timestamp max1 = (ts_src_addr > cdt) ? ts_src_addr : cdt;
        Timestamp max2 = (max1 > ts_src1) ? max1 : ts_src1;
		Timestamp value = max2 + LOAD_COST;

        RShadowSet(value, dest, index);
        RegionUpdateCp(region, value);

        MSG(2, "logLoadInst1Src level %u version %u \n", i, RegionGetVersion(i));
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
    MSG(1, "store ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);
    if (!isKremlinOn())
    	return NULL;

    addWork(STORE_COST);

#ifndef WORK_ONLY
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();
    Level i;

    for (i = minLevel; i <= maxLevel; i++) {
		Region* region = RegionGet(i);
		Index index = getIndex(i);
		Version version = RegionGetVersion(i);
        Timestamp cdt = CDepGet(index);
		Timestamp ts0 = RShadowGet(src, index);
        Timestamp greater1 = (cdt > ts0) ? cdt : ts0;
        Timestamp value = greater1 + STORE_COST;
#ifdef EXTRA_STATS
        region->storeCnt++;
        //updateWriteMemoryAccess(entryDest, i, RegionGetVersion(i), value);
#endif
		MShadowSet(dest_addr, index, version, value);
        RegionUpdateCp(region, value);
    }

#endif
    //return entryDest;
    return NULL;
}


void* logStoreInstConst(Addr dest_addr) {
    MSG(1, "storeConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    if (!isKremlinOn())
        return NULL;

    addWork(STORE_COST);

#ifndef WORK_ONLY

#ifdef EXTRA_STATS
    //TEntry* entryLine = getGTEntryCacheLine(dest_addr);
#endif
    
    //fprintf(stderr, "\nstoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();
    Level i;
    for (i = minLevel; i <= maxLevel; ++i) {
		Region* region = RegionGet(i);
		Index index = getIndex(i);
		Version version = RegionGetVersion(i);
        Timestamp cdt = CDepGet(index);
        Timestamp value = cdt + STORE_COST;
#ifdef EXTRA_STATS
        //updateWriteMemoryAccess(entryDest, i, version, value);
        //updateWriteMemoryLineAccess(entryLine, i, version, value);
#endif
		MShadowSet(dest_addr, index, version, value);
		RegionUpdateCp(region, value);
    }
#endif
    return NULL;
}


// This function is called before 
// callee's LogRegionEnter is called.
// Save the return register name in caller's context

void addReturnValueLink(Reg dest) {
    MSG(1, "prepare return storage ts[%u]\n", dest);
    if (!isKremlinOn())
        return;
#ifndef WORK_ONLY
	FuncContext* caller = RegionGetFunc();
	caller->ret = dest;	
#endif
}

// This is called right before callee's "logRegionExit"
// read timestamp of the callee register and 
// update the caller register that will hold the return value
//
void logFuncReturn(Reg src) {
    MSG(1, "write return value ts[%u]\n", src);
    if (!isKremlinOn())
        return;

#ifndef WORK_ONLY
    FuncContext* callee = RegionGetFunc();
    FuncContext* caller = RegionGetCallerFunc();

	// main function does not have a return point
	if (caller == NULL)
		return;

	assert(caller->ret >= 0);
	int indexSize = caller->table->indexSize;
	RShadowCopy(caller->table, caller->ret, callee->table, src, 0, indexSize);
	
    MSG(1, "end write return value 0x%x\n", RegionGetFunc());
#endif
}

void logFuncReturnConst(void) {
    MSG(1, "logFuncReturnConst\n");
    if (!isKremlinOn())
        return;

#ifndef WORK_ONLY
    int i;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    // Assert there is a function context before the top.
	FuncContext* caller = RegionGetCallerFunc();

	// main function does not have a return point
	if (caller == NULL)
		return;

    // Skip of the caller did not set up a return value location (i.e. lib functions).
    //if(!(*nextHead)->ret) return;

    for (i = minLevel; i <= maxLevel; i++) {
		Index index = getIndex(i);
        UInt64 cdt = CDepGet(index);
		RShadowSetWithTable(caller->table, cdt, caller->ret, index);
    }
#endif
}

#if 0
void logBBVisit(UInt bb_id) {
    if (!isKremlinOn()) return;

#ifdef MANAGE_BB_INFO
    MSG(1, "logBBVisit(%u)\n", bb_id);
    __prevBB = __currentBB;
    __currentBB = bb_id;
#endif
}
#endif

// this function is the same as logAssignmentConst but helps to quickly
// identify induction variables in the source code
void* logInductionVar(UInt dest) {
    MSG(1, "logInductionVar to %u\n", dest);
    if (!isKremlinOn())
		return NULL;
    return logAssignmentConst(dest);
}

/******************************************************************
 * logPhi Functions
 *
 *  for the efficiency, we use several versions with different 
 *  number of incoming dependences
 ******************************************************************/

void* logPhiNode1CD(UInt dest, UInt src, UInt cd) {
    MSG(1, "logPhiNode1CD ts[%u] = max(ts[%u], ts[%u])\n", dest, src, cd);
    if (!isKremlinOn())
		return NULL;


#ifndef WORK_ONLY
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
		Index index = getIndex(i);
		Timestamp ts_src = RShadowGet(src, index);
		Timestamp ts_cd = RShadowGet(cd, index);
        Timestamp max = (ts_src > ts_cd) ? ts_src : ts_cd;
        //updateTimestamp(entryDest, i, version, max);
		RShadowSet(max, dest, index);
        MSG(2, "logPhiNode1CD level %u version %u \n", i, RegionGetVersion(i));
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
    MSG(1, "logPhiNode2CD ts[%u] = max(ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2);
    if (!isKremlinOn())
    	return NULL;

#ifndef WORK_ONLY
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    int i;
    for (i = minLevel; i <= maxLevel; i++) {
		Index index = getIndex(i);
		Timestamp ts_src = RShadowGet(src, index);
		Timestamp ts_cd1 = RShadowGet(cd1, index);
		Timestamp ts_cd2 = RShadowGet(cd2, index);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;

		RShadowSet(max2, dest, index);

        MSG(2, "logPhiNode2CD level %u version %u \n", i, RegionGetVersion(i));
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
    MSG(1, "logPhiNode3CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3);

    if (!isKremlinOn())
    	return NULL;


#ifndef WORK_ONLY

    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
		Index index = getIndex(i);
		Timestamp ts_src = RShadowGet(src, index);
		Timestamp ts_cd1 = RShadowGet(cd1, index);
		Timestamp ts_cd2 = RShadowGet(cd2, index);
		Timestamp ts_cd3 = RShadowGet(cd3, index);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;

		RShadowSet(max3, dest, index);

        MSG(2, "logPhiNode3CD level %u version %u \n", i, RegionGetVersion(i));
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
    MSG(1, "logPhiNode4CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest, src, cd1, cd2, cd3, cd4);

    if (!isKremlinOn())
    	return NULL;

#ifndef WORK_ONLY
    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
		Index index = getIndex(i);
		Timestamp ts_src = RShadowGet(src, index);
		Timestamp ts_cd1 = RShadowGet(cd1, index);
		Timestamp ts_cd2 = RShadowGet(cd2, index);
		Timestamp ts_cd3 = RShadowGet(cd3, index);
		Timestamp ts_cd4 = RShadowGet(cd4, index);
        Timestamp max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Timestamp max4 = (max3 > ts_cd4) ? max3 : ts_cd4;

		RShadowSet(max4, dest, index);

        MSG(2, "logPhiNode4CD level %u version %u \n", i, RegionGetVersion(i));
        MSG(2, " src %u cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", src, cd1, cd2, cd3, cd4, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", ts_src, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
    }

    //return entryDest;
    return NULL;
#else
    return NULL;
#endif
}

void* log4CDepoPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    MSG(1, "log4CDepoPhiNode ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest, dest, cd1, cd2, cd3, cd4);

    if (!isKremlinOn())
		return NULL;

#ifndef WORK_ONLY
    int i = 0;
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();

    for (i = minLevel; i <= maxLevel; i++) {
		Index index = getIndex(i);
        Timestamp ts_dest = RShadowGet(dest, index);
		Timestamp ts_cd1 = RShadowGet(cd1, index);
		Timestamp ts_cd2 = RShadowGet(cd2, index);
		Timestamp ts_cd3 = RShadowGet(cd3, index);
		Timestamp ts_cd4 = RShadowGet(cd4, index);
        Timestamp max1 = (ts_dest > ts_cd1) ? ts_dest : ts_cd1;
        Timestamp max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Timestamp max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Timestamp max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
		RShadowSet(max4, dest, index);

        MSG(2, "log4CDepoPhiNode4CD level %u version %u \n", i, RegionGetVersion(i));
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
    MSG(1, "logPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest, src, dest);

    if (!isKremlinOn())
    	return;

#ifndef WORK_ONLY
    int minLevel = getStartLevel();
    int maxLevel = getEndLevel();
    int i = 0;

    for (i = minLevel; i <= maxLevel; i++) {
		Index index = getIndex(i);
		Region* region = RegionGet(i);
		Timestamp ts0 = RShadowGet(src, index);
		Timestamp ts1 = RShadowGet(dest, index);
        Timestamp value = (ts0 > ts1) ? ts0 : ts1;
		RShadowSet(value, dest, index);
        RegionUpdateCp(region, value);
        MSG(2, "logPhiAddCond level %u version %u \n", i, RegionGetVersion(i));
        MSG(2, " src %u dest %u\n", src, dest);
        MSG(2, " ts0 %u ts1 %u value %u\n", ts0, ts1, value);
    }
	
#endif
}



/******************************
 * Kremlin Init / Deinit
 *****************************/

static UInt hasInitialized = 0;

static void initInternals() {
	}

static void deinitInternals() {
	}

Bool kremlinInit() {
	DebugInit("kremlin.log");
    if(hasInitialized++) {
        MSG(0, "kremlinInit skipped\n");
        return FALSE;
    }

    MSG(0, "Profile Level = (%d, %d), Index Size = %d\n", 
        getMinLevel(), getMaxLevel(), getIndexSize());

    MSG(0, "kremlinInit running....");


	if(getKremlinDebugFlag()) { 
		fprintf(stderr,"[kremlin] debugging enabled at level %d\n", getKremlinDebugLevel()); 
	}

#if 0
    InvokeRecordsCreate(&invokeRecords);
#endif


    MemMapAllocatorCreate(&memPool, ALLOCATOR_SIZE);
	ArgFifoInit();
	CDepInit();
	CRegionInit();
	RShadowInit(getIndexSize());
	MShadowInit();
	RegionInit();
   	turnOnProfiler();
    return TRUE;
}




Bool kremlinDeinit() {
    if(--hasInitialized) {
        MSG(0, "kremlinDeinit skipped\n");
        return FALSE;
    }

	turnOffProfiler();
	CRegionDeinit("kremlin.bin");
	RShadowDeinit();
	MShadowDeinit();
	CDepDeinit();
	ArgFifoDeinit();
	RegionDeinit();
    MemMapAllocatorDelete(&memPool);

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
        UInt version = RegionGetVersion(i);
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
        UInt64 value = CDepGet(i) + FREE_COST;

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
	assert(0);
    //printf("Warning: logInsertValue not correctly implemented\n");
    return logAssignment(src, dest);
}

void* logInsertValueConst(UInt dest) {
	assert(0);
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

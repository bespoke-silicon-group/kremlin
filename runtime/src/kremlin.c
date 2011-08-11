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

#include "RShadow.c"

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)
#define DS_ALLOC_SIZE   100     // used for static data structures
#define MAX_SRC_TSA_VAL	6

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))

#define isKremlinOn()		(kremlinOn == 1)


static Bool 	kremlinOn = 0;
static UInt64	loadCnt = 0llu;
static UInt64	storeCnt = 0llu;


UInt64 _regionFuncCnt;
UInt64 _setupTableCnt;
int _requireSetupTable;

/*****************************************************************
 * Region Level Management 
 *****************************************************************/


// min and max level for instrumentation
Level __kremlin_min_level = 0;
Level __kremlin_max_level = 21;	 


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


static Index nInstrument = 0;
// what's the index depth currently being used?
// can be optimized
static inline Index getIndexDepth() {
	return nInstrument;
}

static inline void updateIndexDepth(Level newLevel) {
	if (newLevel < getMinLevel()) {
		nInstrument = 0;
		return;
	}

	nInstrument = getEndLevel() -getStartLevel() + 1;	
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
	updateInstrumentable(levelNum);
	updateIndexDepth(levelNum);
}

static inline void decrementRegionLevel() {
	levelNum--; 
	updateInstrumentable(levelNum);
	updateIndexDepth(levelNum);
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
 * VArray / TArray Management
 *************************************************************/

static int arraySize = 512;
static Version* vArray;
static Time* tArray;

static void RegionInitVersion() {
	vArray = (Version*) calloc(sizeof(Version), arraySize); 
}

static void RegionInitTArray() {
	tArray = (Time*) calloc(sizeof(Time), arraySize); 
}

static inline Time* RegionGetTArray() {
	return &tArray[0];
}


static inline Version* RegionGetVArray(Level level) {
	return &vArray[level];
}

static inline void RegionIssueVersion(Level level) {
	vArray[level]++;	
}

static inline Version RegionGetVersion(Level level) {
	return vArray[level];
}

#if 0
static void RegionReallocArrays(int newSize) {
	if (arraySize >= newSize) {
		return;
	}
	int oldArraySize = arraySize;
	arraySize *= 2;

	Version* old = vArray;
	vArray = (Version*) calloc(sizeof(Version), arraySize); 
	memcpy(vArray, old, sizeof(Version), oldArraySize);
	tArray = (Time*) calloc(sizeof(Time), arraySize); 
}
#endif





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
 *****************************************************************/

static CID lastCallSiteId;

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

    MSG(0, "RegionPushFunc at 0x%x CID 0x%x\n", funcContext, cid);
	//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);
}

/**
 * Removes context at the top of the function context stack.
 */
static void RegionPopFunc() {
    FuncContext* func = FuncContextsPopVal(funcContexts);
    assert(func);
    MSG(0, "RegionPopFunc at 0x%x CID 0x%x\n", func, func->callSiteId);

    assert(_regionFuncCnt == _setupTableCnt);
    assert(_requireSetupTable == 0);

    if (func->table != NULL)
        RShadowFreeTable(func->table);

    free(func);  
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
	if (FuncContextsSize(funcContexts) == 1) {
    	MSG(3, "RegionGetCallerFunc  No Caller Context\n");
		return NULL;
	}
    FuncContext** func = FuncContextsLast(funcContexts) - 1;
    MSG(3, "RegionGetCallerFunc  0x%x CID 0x%x\n", *func, (*func)->callSiteId);
	assert((*func)->code == 0xDEADBEEF);
	return *func;
}

inline static void RegionSetRetReg(FuncContext* func, Reg reg) {
	func->ret = reg;	
}

inline static Reg RegionGetRetReg(FuncContext* func) {
	return func->ret;
}

inline static LTable* RegionGetTable(FuncContext* func) {
	return func->table;
}

static void RegionInitFunc()
{
    FuncContextsCreate(&funcContexts);
    assert(FuncContextsEmpty(funcContexts));
}

static void RegionDeinitFunc()
{
    assert(FuncContextsEmpty(funcContexts));
    FuncContextsDelete(&funcContexts);
}

/*****************************************************************
 * Region Management
 *****************************************************************/
#if 0
typedef struct _CDep {
	Time* time;
	int size;	
	int nextWriteIndex;
	Time* current;
} CDep;


#define CDEP_INIT_SIZE	64

void CDepAlloc(CDep* dep) {
	dep->size = CDEP_INIT_SIZE;
	dep->nextWriteIndex = 0;
	dep->time = malloc(sizeof(Time) * dep->size);
}

void CDepRealloc(CDep* dep) {
	int oldSize = dep->size;
	assert(dep->nextWriteIndex == oldSize);
	dep->size *= 2;
	dep->time = realloc(dep->time, sizeof(Time) * dep->size);
}

void CDepFree(CDep* dep) {
	free(dep->time);
}
#endif

typedef struct _region_t {
	UInt32 code;
	Version version;
	SID	regionId;
	RegionType regionType;
	Time start;
	Time cp;
	Time childrenWork;
	Time childrenCP;
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
static int regionSize;

static int RegionSize() {
	return regionSize;
}

static void RegionInit(int size) {
    regionInfo = (Region*) malloc(sizeof(Region) * size);
	regionSize = size;
	assert(regionInfo != NULL);
	MSG(0, "RegionInit at 0x%x\n", regionInfo);

	int i;
	for (i=0; i<size; i++) {
		regionInfo[i].code = 0xDEADBEEF;
		regionInfo[i].version = 0;
	}
    assert(regionInfo);
	RegionInitVersion();
	RegionInitTArray();
	RegionInitFunc();
}

static void RegionRealloc() {
	int oldRegionSize = RegionSize();
	Region* oldRegionInfo = regionInfo;
	regionSize *= 2;
	MSG(0, "RegionRealloc from %d to %d\n", oldRegionSize, regionSize);
	regionInfo = (Region*) realloc(regionInfo, sizeof(Region) * RegionSize());
	
	int i;
	for (i=oldRegionSize; i<regionSize; i++) {
		regionInfo[i].code = 0xDEADBEEF;
		regionInfo[i].version = 0;
	}
}

static inline Region* RegionGet(Level level) {
	Region* ret = &regionInfo[level];
	return ret;
}

#if 0
static Version RegionGetVersion(Level level) {
	return RegionGet(level)->version;
}
#endif

static void RegionDeinit() {
	RegionDeinitFunc();
	Level i;
	for (i=0; i<regionSize; i++) {
		Region* region = RegionGet(i);
	}
	assert(regionInfo != NULL);
    regionInfo = NULL;
}

static inline void RegionUpdateCp(Region* region, Timestamp value) {
	region->cp = MAX(value, region->cp);
	assert(value <= getTimetick() - region->start);
}


void checkRegion() {
#if 0
	int i;
	int bug = 0;
	for (i=0; i<RegionSize(); i++) {
		Region* ret = &regionInfo[i];
		if (ret->code != 0xDEADBEEF) {
			MSG(0, "Region Error at index %d\n", i);	
			assert(0);
			assert(ret->code == 0xDEADBEEF);
		}
	}
	if (bug > 0)
		assert(0);
#endif
}

static inline void RegionRestart(Region* region, SID sid, UInt regionType, Level level) {
	//region->version++;
	RegionIssueVersion(level);
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

LTable* cTable;
int cTableReadPtr = 0;
Time* cTableCurrentBase;

#define CDEP_INIT_ENTRY	256
#define CDEP_INIT_INDEX	64

inline void CDepInit() {
	cTableReadPtr = 0;
	cTable = RShadowCreateTable(CDEP_INIT_ENTRY, CDEP_INIT_INDEX);
}

inline void CDepDeinit() {
	RShadowFreeTable(cTable);
}

inline void CDepInitRegion(Index index) {
	assert(cTable != NULL);
	MSG(0, "CDepInitRegion ReadPtr = %d, Index = %d\n", cTableReadPtr, index);
	RShadowSetWithTable(cTable, 0ULL, cTableReadPtr, index);
	cTableCurrentBase = RShadowGetElementAddr(cTable, cTableReadPtr, 0);
}

inline Time CDepGet(Index index) {
	assert(cTable != NULL);
	assert(cTableReadPtr >=  0);
	return *(cTableCurrentBase + index);
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
	cTableReadPtr++;
	int indexSize = getIndexDepth();

// TODO: rarely, ctable could require resizing..not implemented yet
	if (cTableReadPtr == cTable->entrySize) {
		fprintf(stderr, "CDep Table requires entry resizing..\n");
		assert(0);	
	}

	if (cTable->indexSize < indexSize) {
		fprintf(stderr, "CDep Table requires index resizing..\n");
		assert(0);	
	}

	LTable* ltable = RShadowGetTable();
	assert(lTable->indexSize >= indexSize);
	assert(cTable->indexSize >= indexSize);

	RShadowCopy(cTable, cTableReadPtr, lTable, cond, 0, indexSize);
	cTableCurrentBase = RShadowGetElementAddr(cTable, cTableReadPtr, 0);
	assert(cTableReadPtr < cTable->entrySize);
#endif
}

void removeControlDep() {
    MSG(1, "pop  ControlDep\n");
    if (!isKremlinOn()) {
		return;
	}
#ifndef WORK_ONLY
	cTableReadPtr--;
	cTableCurrentBase = RShadowGetElementAddr(cTable, cTableReadPtr, 0);
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
    MSG(0, "transfer arg data to ts[%u] \n", dest);
    if (!isKremlinOn())
        return;

	Reg src = ArgFifoPop();
	// copy parent's src timestamp into the currenf function's dest reg
	if (src != DUMMY_ARG) {
		FuncContext* caller = RegionGetCallerFunc();
		FuncContext* callee = RegionGetFunc();
		LTable* callerT = RegionGetTable(caller);
		LTable* calleeT = RegionGetTable(callee);

		// decrement one as the current level should not be copied
		int indexSize = getIndexDepth() - 1;
		assert(getCurrentLevel() >= 1);
		RShadowCopy(calleeT, dest, callerT, src, 0, indexSize);
	}
    MSG(0, "\n", dest);
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
	if (level == RegionSize()) {
		RegionRealloc();
	}
	
	Region* region = RegionGet(level);
	RegionRestart(region, regionId, regionType, level);

	MSG(0, "\n");
	MSG(0, "[+++] region [type %u, level %d, sid 0x%llx] start: %llu\n",
        regionType, level, regionId, getTimetick());
    incIndentTab(); // only affects debug printing


	// func region allocates a new RShadow Table.
	// for other region types, it needs to "clean" previous region's timestamps
    if(regionType == RegionFunc) {
		_regionFuncCnt++;
        RegionPushFunc(lastCallSiteId);
        _requireSetupTable = 1;

    } else {
		if (isInstrumentable())
			RShadowRestartIndex(getIndex(level));
	}

    FuncContext* funcHead = RegionGetFunc();
	CID callSiteId = (funcHead == NULL) ? 0x0 : funcHead->callSiteId;
	CRegionEnter(regionId, callSiteId);

#ifndef WORK_ONLY
	if (isInstrumentable()) {
    	//RegionPushCDep(region, 0);
		CDepInitRegion(getIndex(level));
		assert(CDepGet(getIndex(level)) == 0ULL);
	}
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
    MSG(0, "[---] region [type %u, level %u, sid 0x%llx] time %llu cp %llu work %llu\n",
        regionType, level, regionId, getTimetick(), region->cp, work);

	assert(region->regionId == regionId);
    UInt64 cp = region->cp;

#ifdef KREMLIN_DEBUG
	if (work < cp) {
		fprintf(stderr, "work = %llu\n", work);
		fprintf(stderr, "cp = %llu\n", cp);
		assert(0);
	}
	if (level < getMaxLevel() && level >= getMinLevel()) {
		assert(work >= cp);
		assert(work >= region->childrenWork);
	}
#endif

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

	double spTemp = (work - region->childrenWork + region->childrenCP) / (double)cp;
	double sp = (work > 0) ? spTemp : 1.0;

#ifdef KREMLIN_DEBUG
	// Check that cp is positive if work is positive.
	// This only applies when the current level gets instrumented (otherwise this condition always holds)
    if (isInstrumentable() && cp == 0 && work > 0) {
        fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
        fprintf(stderr, "region [type: %u, level: %u, sid: %llu] parent [%llu] cp %llu work %llu\n",
            regionType, level, regionId,  parentSid,  region->cp, work);
        assert(0);
    }

	if (level < getMaxLevel() && sp < 1.0) {
		fprintf(stderr, "sid=%lld work=%llu childrenWork = %llu childrenCP=%lld\n", sid, work,
			region->childrenWork, region->childrenCP);
		assert(0);
	}
#endif

	UInt64 spWork = (UInt64)((double)work / sp);
	UInt64 tpWork = cp;

	// due to floating point variables,
	// spWork or tpWork can be larger than work
	if (spWork > work) { spWork = work; }
	if (tpWork > work) { tpWork = work; }

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
	Index index;

    for (index = 0; index < getIndexDepth(); index++) {
		// CDep and shadow memory are index based
		Level i = getLevel(index);
		Region* region = RegionGet(i);
        //Time cdt = RegionGetCDep(region);
		Time cdt = CDepGet(index);
		assert(cdt <= getTimetick() - region->start);
        Time ts0 = RShadowGet(src0, index);
        Time ts1 = RShadowGet(src1, index);
        Time greater0 = (ts0 > ts1) ? ts0 : ts1;
        Time greater1 = (cdt > greater0) ? cdt : greater0;
        Time value = greater1 + opCost;
		RShadowSet(value, dest, index);

		// region info is level based
		
        MSG(3, "binOp[%u] level %u version %u \n", opCost, i, RegionGetVersion(i));
        MSG(3, " src0 %u src1 %u dest %u\n", src0, src1, dest);
        MSG(3, " ts0 %u ts1 %u cdt %u value %u\n", ts0, ts1, cdt, value);
        RegionUpdateCp(region, value);
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
	LTable* table = RShadowGetTable();
	Time* base = table->array; 
	int unit = table->indexSize;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		assert(cdt <= getTimetick() - region->start);
        Time ts0 = RShadowGet(src, index);
        Time greater1 = (cdt > ts0) ? cdt : ts0;
        Time value = greater1 + opCost;
		RShadowSet(value, dest, index);

        MSG(1, "binOpConst[%u] level %u version %u \n", opCost, i, RegionGetVersion(i));
	    MSG(1, " src %u dest %u\n", src, dest);
   	    MSG(1, " ts0 %u cdt %u value %u\n", ts0, cdt, value);
		RegionUpdateCp(region, value);
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
	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		RShadowSet(cdt, dest, index);
        RegionUpdateCp(region, cdt);
    }
#endif
    return NULL;
}

void* logLoadInst(Addr addr, Reg dest) {
    MSG(0, "load ts[%u] = ts[0x%x] + %u\n", dest, addr, LOAD_COST);
    if (!isKremlinOn())
    	return NULL;

    addWork(LOAD_COST);

#ifndef WORK_ONLY
	Index index;
	Time* tArray = RegionGetTArray();
	Index depth = getIndexDepth();
	Level minLevel = getLevel(0);
	MShadowGet(addr, depth, RegionGetVArray(minLevel), tArray);

    for (index = 0; index < depth; index++) {
		Level i = getLevel(i);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		Time ts0 = tArray[index];
        Time greater1 = (cdt > ts0) ? cdt : ts0;
        Time value = greater1 + LOAD_COST;

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

void* logLoadInst1Src(Addr addr, UInt src1, UInt dest) {
    MSG(0, "load ts[%u] = max(ts[0x%x],ts[%u]) + %u\n", dest, addr, src1, LOAD_COST);
    if (!isKremlinOn())
		return NULL;

    addWork(LOAD_COST);

#ifndef WORK_ONLY
    Level minLevel = getStartLevel();
    Level maxLevel = getEndLevel();

	Index index;
	Time* tArray = RegionGetTArray();
	MShadowGet(addr, maxLevel - minLevel + 1, RegionGetVArray(minLevel), tArray);

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		Time tsAddr = tArray[index];
		Time tsSrc1 = RShadowGet(src1, index);

        Time max1 = (tsAddr > cdt) ? tsAddr : cdt;
        Time max2 = (max1 > tsSrc1) ? max1 : tsSrc1;
		Time value = max2 + LOAD_COST;

        RShadowSet(value, dest, index);
        RegionUpdateCp(region, value);

        MSG(2, "logLoadInst1Src level %u version %u \n", i, RegionGetVersion(i));
        MSG(2, " addr 0x%x src1 %u dest %u\n", addr, src1, dest);
        MSG(2, " cdt %u tsAddr %u tsSrc1 %u max %u\n", cdt, tsAddr, tsSrc1, max2);
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
    MSG(0, "store ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);
    if (!isKremlinOn())
    	return NULL;

    addWork(STORE_COST);

#ifndef WORK_ONLY
	Index index;
	Time* tArray = RegionGetTArray();

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		Time ts0 = RShadowGet(src, index);
        Time greater1 = (cdt > ts0) ? cdt : ts0;
        Time value = greater1 + STORE_COST;
		tArray[index] = value;
#ifdef EXTRA_STATS
        region->storeCnt++;
        //updateWriteMemoryAccess(entryDest, i, RegionGetVersion(i), value);
#endif
        RegionUpdateCp(region, value);
    }

	Level minLevel = getLevel(0);
	MShadowSet(dest_addr, getIndexDepth(), RegionGetVArray(minLevel), tArray);
#endif
    //return entryDest;
    return NULL;
}


void* logStoreInstConst(Addr dest_addr) {
    MSG(0, "storeConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
    if (!isKremlinOn())
        return NULL;

    addWork(STORE_COST);

#ifndef WORK_ONLY

#ifdef EXTRA_STATS
    //TEntry* entryLine = getGTEntryCacheLine(dest_addr);
#endif
    
    //fprintf(stderr, "\nstoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
	Index index;
	Time* tArray = RegionGetTArray();

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Index index = getIndex(i);
		Time cdt = CDepGet(index);
        Time value = cdt + STORE_COST;
		tArray[index] = value;
#ifdef EXTRA_STATS
        //updateWriteMemoryAccess(entryDest, i, version, value);
        //updateWriteMemoryLineAccess(entryLine, i, version, value);
#endif
		RegionUpdateCp(region, value);
    }
	Level minLevel = getLevel(0);
	MShadowSet(dest_addr, getIndexDepth(), RegionGetVArray(minLevel), tArray);
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
	RegionSetRetReg(caller, dest);
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

	Reg ret = RegionGetRetReg(caller);
	assert(ret >= 0);

	// current level time does not need to be copied
	int indexSize = getIndexDepth() - 1;
	RShadowCopy(caller->table, ret, callee->table, src, 0, indexSize);
	
    MSG(1, "end write return value 0x%x\n", RegionGetFunc());
#endif
}

void logFuncReturnConst(void) {
    MSG(1, "logFuncReturnConst\n");
    if (!isKremlinOn())
        return;

#ifndef WORK_ONLY

    // Assert there is a function context before the top.
	FuncContext* caller = RegionGetCallerFunc();

	// main function does not have a return point
	if (caller == NULL)
		return;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Time cdt = CDepGet(index);
		RShadowSetWithTable(caller->table, cdt, RegionGetRetReg(caller), index);
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
	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Time ts_src = RShadowGet(src, index);
		Time ts_cd = RShadowGet(cd, index);
        Time max = (ts_src > ts_cd) ? ts_src : ts_cd;
		RShadowSet(max, dest, index);
        MSG(3, "logPhiNode1CD level %u version %u \n", i, RegionGetVersion(i));
        MSG(3, " src %u cd %u dest %u\n", src, cd, dest);
        MSG(3, " ts_src %u ts_cd %u max %u\n", ts_src, ts_cd, max);
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
	Index index;

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Time ts_src = RShadowGet(src, index);
		Time ts_cd1 = RShadowGet(cd1, index);
		Time ts_cd2 = RShadowGet(cd2, index);
        Time max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Time max2 = (max1 > ts_cd2) ? max1 : ts_cd2;

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
	Index index;

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Time ts_src = RShadowGet(src, index);
		Time ts_cd1 = RShadowGet(cd1, index);
		Time ts_cd2 = RShadowGet(cd2, index);
		Time ts_cd3 = RShadowGet(cd3, index);
        Time max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Time max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Time max3 = (max2 > ts_cd3) ? max2 : ts_cd3;

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
	Index index;

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Time ts_src = RShadowGet(src, index);
		Time ts_cd1 = RShadowGet(cd1, index);
		Time ts_cd2 = RShadowGet(cd2, index);
		Time ts_cd3 = RShadowGet(cd3, index);
		Time ts_cd4 = RShadowGet(cd4, index);
        Time max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Time max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Time max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Time max4 = (max3 > ts_cd4) ? max3 : ts_cd4;

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

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    MSG(1, "log4CDToPhiNode ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest, dest, cd1, cd2, cd3, cd4);

    if (!isKremlinOn())
		return NULL;

#ifndef WORK_ONLY
	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
        Time ts_dest = RShadowGet(dest, index);
		Time ts_cd1 = RShadowGet(cd1, index);
		Time ts_cd2 = RShadowGet(cd2, index);
		Time ts_cd3 = RShadowGet(cd3, index);
		Time ts_cd4 = RShadowGet(cd4, index);
        Time max1 = (ts_dest > ts_cd1) ? ts_dest : ts_cd1;
        Time max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Time max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Time max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
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
	Index index;

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time ts0 = RShadowGet(src, index);
		Time ts1 = RShadowGet(dest, index);
        Time value = (ts0 > ts1) ? ts0 : ts1;
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

#define REGION_INIT_SIZE	64

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
	RegionInit(REGION_INIT_SIZE);
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
	ArgFifoDeinit();
	CDepDeinit();
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

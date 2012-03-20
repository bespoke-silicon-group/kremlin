#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "kremlin.h"
#include "arg.h"
#include "debug.h"
#include "kremlin_deque.h"
#include "CRegion.h"
#include "hash_map.h"
#include "Vector.h"
#include "MShadow.h"
#include "RShadow.h"
#include "RShadow.c"
#include "interface.h"

//#include "idbg.h"
#define LOAD_COST           4
#define STORE_COST          1
#define MALLOC_COST         100
#define FREE_COST           10

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))

#define isKremlinOn()		(kremlinOn == 1)


static Bool 	kremlinOn = 0;
static UInt64	loadCnt = 0llu;
static UInt64	storeCnt = 0llu;


static UInt64 _regionFuncCnt;
static UInt64 _setupTableCnt;
static int _requireSetupTable;


Time* (*MShadowGet)(Addr, Index, Version*, UInt32) = NULL;
void  (*MShadowSet)(Addr, Index, Version*, Time*, UInt32) = NULL;

/*****************************************************************
 * Region Level Management 
 *****************************************************************/


// min and max level for instrumentation
Level __kremlin_min_level;
Level __kremlin_max_level;	 
Level __kremlin_max_active_level = 0;	 
Level __kremlin_index_size;



static inline int getMinLevel() {
	return __kremlin_min_level;
}

static inline int getMaxLevel() {
	return __kremlin_max_level;
}

static inline int initLevels() {
	__kremlin_min_level = KConfigGetMinLevel();
	__kremlin_max_level = KConfigGetMaxLevel();
	__kremlin_index_size = KConfigGetMaxLevel() - KConfigGetMinLevel() + 1;
}

static inline int getLevel(Index index) {
	return __kremlin_min_level + index;
}

static inline int getIndexSize() {
	return __kremlin_index_size;
}

static Level levelNum = -1;

static inline Level getCurrentLevel() {
	return levelNum;
}

inline void updateMaxActiveLevel(Level level) {
	if (level > __kremlin_max_active_level)
		__kremlin_max_active_level = level;
}

Level getMaxActiveLevel() {
	return __kremlin_max_active_level;
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
static Version nextVersion = 0;

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
	vArray[level] = nextVersion++;	
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
 * 1) caller calls "_KPrepCall" to reset argFifo
 * 2) for each arg, the caller calls _KLinkArg or _KLinkArgConst
 * 3) callee function enters with "_KEnterRegion"
 * 4) callee links each arg with _KUnlinkArg by 
 * dequeing a register number from fifo,
 * in the same order used in _KLinkArg
 *
 * FIFO size is conservatively set to 64, 
 * which is already very large for # of args for a function call.
 *
 * Is single FIFO enough in Kremlin?
 *   Yes, function args will be prepared and processed 
 *   before and after KEnterRegion.
 *   The fifo can be reused for the next function calls.
 *   
 * Why reset the queue every time?
 *   uninstrumented functions (e.g. library) do not have 
 *  "_KUnlinkArg" call, so there could be 
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
	Table* table;
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
    //FuncContext* funcContext = (FuncContext*) malloc(sizeof(FuncContext));
    FuncContext* funcContext = (FuncContext*) MemPoolAllocSmall(sizeof(FuncContext));
    assert(funcContext);

    FuncContextsPushVal(funcContexts, funcContext);
    funcContext->table = NULL;
	funcContext->callSiteId = cid;
	funcContext->ret = DUMMY_RET;
	funcContext->code = 0xDEADBEEF;

    MSG(3, "RegionPushFunc at 0x%x CID 0x%x\n", funcContext, cid);
	//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);
}

/**
 * Removes context at the top of the function context stack.
 */
static void RegionPopFunc() {
    FuncContext* func = FuncContextsPopVal(funcContexts);
    assert(func);
    MSG(3, "RegionPopFunc at 0x%x CID 0x%x\n", func, func->callSiteId);

    assert(_regionFuncCnt == _setupTableCnt);
    assert(_requireSetupTable == 0);

    if (func->table != NULL)
        TableFree(func->table);

    //free(func);  
	MemPoolFreeSmall(func, sizeof(FuncContext));
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

inline static Table* RegionGetTable(FuncContext* func) {
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
typedef struct _region_t {
	UInt32 code;
	Version version;
	SID	regionId;
	RegionType regionType;
	Time start;
	Time cp;
	Time childrenWork;
	Time childrenCP;
	Time childMaxCP;
	UInt64 childCount;
#ifdef EXTRA_STATS
	UInt64 loadCnt;
	UInt64 storeCnt;
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 writeLineCnt;
#endif
} Region;


static Region* regionInfo = NULL;
static int regionSize;

static int RegionSize() {
	return regionSize;
}

static void RegionInit(int size) {
    regionInfo = (Region*) malloc(sizeof(Region) * size);
    //regionInfo = (Region*) MemPoolAllocSmall(sizeof(Region) * size);
	regionSize = size;
	assert(regionInfo != NULL);
	MSG(3, "RegionInit at 0x%x\n", regionInfo);

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
	fprintf(stderr, "Region Realloc..new size = %d\n", regionSize);
	MSG(3, "RegionRealloc from %d to %d\n", oldRegionSize, regionSize);
	regionInfo = (Region*) realloc(regionInfo, sizeof(Region) * RegionSize());
	
	int i;
	for (i=oldRegionSize; i<regionSize; i++) {
		regionInfo[i].code = 0xDEADBEEF;
		regionInfo[i].version = 0;
	}
}

static inline Region* RegionGet(Level level) {
	assert(level < RegionSize());
	Region* ret = &regionInfo[level];
	assert(ret->code == 0xDEADBEEF);
	return ret;
}

static void RegionDeinit() {
	RegionDeinitFunc();
	Level i;
	for (i=0; i<regionSize; i++) {
		Region* region = RegionGet(i);
	}
	assert(regionInfo != NULL);
    regionInfo = NULL;
}

static inline void checkTimestamp(int index, Region* region, Timestamp value) {
#ifndef NDEBUG
	if (value > getTimetick() - region->start) {
		fprintf(stderr, "index = %d, value = %lld, getTimetick() = %lld, region start = %lld\n", 
		index, value, getTimetick(), region->start);
		assert(0);
	}
#endif
}

static inline void RegionUpdateCp(Region* region, Timestamp value) {
	region->cp = MAX(value, region->cp);
	MSG(3, "RegionUpdateCp : value = %llu\n", region->cp);	
	assert(region->code == 0xDEADBEEF);
#ifndef NDEBUG
	//assert(value <= getTimetick() - region->start);
	if (value > getTimetick() - region->start) {
		fprintf(stderr, "value = %lld, getTimetick() = %lld, region start = %lld\n", 
		value, getTimetick(), region->start);
		assert(0);
	}
#endif
}


void checkRegion() {
#ifndef NDEBUG
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
	region->childMaxCP = 0LL;
	region->childCount = 0LL;
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



/****************************************************************
 *
 * Kremlib Compiler Interface Functions
 *
 *****************************************************************/

/*****************************************************************
 * Control Dependence Management
 *****************************************************************/
#define CDEP_ROW 256
#define CDEP_COL 64

static Table* cTable;
static int cTableReadPtr = 0;
static Time* cTableCurrentBase;

inline void CDepInit() {
	cTableReadPtr = 0;
	cTable = TableCreate(CDEP_ROW, CDEP_COL);
}

inline void CDepDeinit() {
	TableFree(cTable);
}

inline void CDepInitRegion(Index index) {
	assert(cTable != NULL);
	MSG(3, "CDepInitRegion ReadPtr = %d, Index = %d\n", cTableReadPtr, index);
	TableSetValue(cTable, 0ULL, cTableReadPtr, index);
	cTableCurrentBase = TableGetElementAddr(cTable, cTableReadPtr, 0);
}

inline Time CDepGet(Index index) {
	assert(cTable != NULL);
	assert(cTableReadPtr >=  0);
	return *(cTableCurrentBase + index);
}

void _KPushCDep(Reg cond) {
    MSG(3, "Push CDep ts[%u]\n", cond);
	idbgAction(KREM_ADD_CD,"## KPushCDep(cond=%u)\n",cond);

	checkRegion();
    if (!isKremlinOn()) {
		return;
	}

	cTableReadPtr++;
	int indexSize = getIndexDepth();

// TODO: rarely, ctable could require resizing..not implemented yet
	if (cTableReadPtr == TableGetRow(cTable)) {
		fprintf(stderr, "CDep Table requires entry resizing..\n");
		assert(0);	
	}

	if (TableGetCol(cTable) < indexSize) {
		fprintf(stderr, "CDep Table requires index resizing..\n");
		assert(0);	
	}

	Table* ltable = RShadowGetTable();
	//assert(lTable->col >= indexSize);
	//assert(cTable->col >= indexSize);

	TableCopy(cTable, cTableReadPtr, lTable, cond, 0, indexSize);
	cTableCurrentBase = TableGetElementAddr(cTable, cTableReadPtr, 0);
	assert(cTableReadPtr < cTable->row);
	checkRegion();
}

void _KPopCDep() {
    MSG(3, "Pop CDep\n");
	idbgAction(KREM_REMOVE_CD, "## KPopCDep()\n");

    if (!isKremlinOn()) {
		return;
	}

	cTableReadPtr--;
	cTableCurrentBase = TableGetElementAddr(cTable, cTableReadPtr, 0);
}

/*****************************************************************
 * Function Call Management
 * 
 * Example of a Sample Function call:
 *   ret = foo (a, b);
 *   a) from the caller:
 *      - _KPrepCall(callsiteId, calleeSID);
 *		- _KLinkArg(a);
 *		- _KLinkArg(b);
 *      - _KLinkReturn(ret);
 *		
 *   b) start of the callee:
 *		- _KEnterRegion(sid);
 *      - _KPrepRTable(regSize, maxDepth);
 *		- _KUnlinkArg(a);
 *		- _KUnlinkArg(b);
 *
 *   c) end of the callee:
 *		- _KReturn(..);
 *      - _KExitRegion();
 *
 * 
 *****************************************************************/


void _KPrepCall(CID callSiteId, UInt64 calledRegionId) {
    MSG(1, "KPrepCall\n");
    if (!isKremlinOn()) { 
		return; 
	}

    // Clear off any argument timestamps that have been left here before the
    // call. These are left on the deque because library calls never take
    // theirs off. 
    ArgFifoClear();
	lastCallSiteId = callSiteId;
}

void _KEnqArg(Reg src) {
    MSG(1, "Enque Arg Reg [%u]\n", src);
    if (!isKremlinOn())
        return;
	ArgFifoPush(src);
}

#define DUMMY_ARG		-1
void _KEnqArgConst() {
    MSG(1, "Enque Const Arg\n");
    if (!isKremlinOn())
        return;

	ArgFifoPush(DUMMY_ARG); // dummy arg
}
void _KLinkArg(Reg src) {
	_KEnqArg(src);
}

void _KLinkArgConst() {
	_KEnqArgConst();
}


// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void _KDeqArg(Reg dest) {
    MSG(3, "Deq Arg to Reg[%u] \n", dest);
    if (!isKremlinOn())
        return;

	Reg src = ArgFifoPop();
	// copy parent's src timestamp into the currenf function's dest reg
	if (src != DUMMY_ARG && getIndexDepth() > 0) {
		FuncContext* caller = RegionGetCallerFunc();
		FuncContext* callee = RegionGetFunc();
		Table* callerT = RegionGetTable(caller);
		Table* calleeT = RegionGetTable(callee);

		// decrement one as the current level should not be copied
		int indexSize = getIndexDepth() - 1;
		assert(getCurrentLevel() >= 1);
		TableCopy(calleeT, dest, callerT, src, 0, indexSize);
	}
    MSG(3, "\n", dest);
}

void _KUnlinkArg(Reg dest) {
	_KDeqArg(dest); 
}

/**
 * Setup the local shadow register table.
 * @param maxVregNum	Number of virtual registers to allocate.
 * @param maxNestLevel	Max relative region depth that can touch the table.
 *
 * A RTable is used by regions in the same function. 
 * Fortunately, it is possible to set the size of the table using 
 * both compile-time and runtime information. 
 *  - row: maxVregNum, as each row represents a virtual register
 *  - col: getCurrentLevel() + 1 + maxNestLevel
 *		maxNestLevel represents the max depth of a region that can use 
 *		the RTable. 
 */
void _KPrepRTable(UInt maxVregNum, UInt maxNestLevel) {
	int tableHeight = maxVregNum;
	int tableWidth = getCurrentLevel() + maxNestLevel + 1;
    MSG(1, "KPrep RShadow Table row=%d, col=%d (curLevel=%d, maxNestLevel=%d)\n",
		 tableHeight, tableWidth, getCurrentLevel(), maxNestLevel);

    if (!isKremlinOn()) {
		 return; 
	}

    assert(_requireSetupTable == 1);
    Table* table = TableCreate(tableHeight, tableWidth);
    FuncContext* funcHead = RegionGetFunc();
	assert(funcHead != NULL);
    assert(funcHead->table == NULL);
    funcHead->table = table;
    assert(funcHead->table != NULL);

    RShadowActivateTable(funcHead->table);
    _setupTableCnt++;
    _requireSetupTable = 0;
}

// This function is called before 
// callee's _KEnterRegion is called.
// Save the return register name in caller's context

void _KLinkReturn(Reg dest) {
    MSG(1, "_KLinkReturn with reg %u\n", dest);
	idbgAction(KREM_ARVL,"## _KLinkReturn(dest=%u)\n",dest);

    if (!isKremlinOn())
        return;

	FuncContext* caller = RegionGetFunc();
	RegionSetRetReg(caller, dest);
}

// This is called right before callee's "_KExitRegion"
// read timestamp of the callee register and 
// update the caller register that will hold the return value
//
void _KReturn(Reg src) {
    MSG(1, "_KReturn with reg %u\n", src);
	idbgAction(KREM_FUNC_RETURN,"## _KReturn (src=%u)\n",src);

    if (!isKremlinOn())
        return;

    FuncContext* callee = RegionGetFunc();
    FuncContext* caller = RegionGetCallerFunc();

	// main function does not have a return point
	if (caller == NULL)
		return;

	Reg ret = RegionGetRetReg(caller);
	assert(ret >= 0);

	// current level time does not need to be copied
	int indexSize = getIndexDepth() - 1;
	if (indexSize > 0)
		TableCopy(caller->table, ret, callee->table, src, 0, indexSize);
	
    MSG(1, "end write return value 0x%x\n", RegionGetFunc());
}

void _KReturnConst(void) {
    MSG(1, "_KReturnConst\n");
	idbgAction(KREM_FUNC_RETURN,"## _KReturnConst()\n");
    if (!isKremlinOn())
        return;

    // Assert there is a function context before the top.
	FuncContext* caller = RegionGetCallerFunc();

	// main function does not have a return point
	if (caller == NULL)
		return;

	Index index;
    for (index = 0; index < caller->table->col; index++) {
		Time cdt = CDepGet(index);
		TableSetValue(caller->table, cdt, RegionGetRetReg(caller), index);
    }
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
void _KTurnOn() {
    kremlinOn = 1;
    MSG(0, "_KTurnOn\n");
	fprintf(stderr, "[kremlin] Logging started.\n");
}

/**
 * end profiling
 */
void _KTurnOff() {
    kremlinOn = 0;
    MSG(0, "_KTurnOff\n");
	fprintf(stderr, "[kremlin] Logging stopped.\n");
}


/*****************************************************************
 * logRegionEntry / logRegionExit
 *****************************************************************/

void _KEnterRegion(SID regionId, RegionType regionType) {
	iDebugHandlerRegionEntry(regionId);
	idbgAction(KREM_REGION_ENTRY,"## KEnterRegion(regionID=%llu,regionType=%u)\n",regionId,regionType);

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

	updateMaxActiveLevel(level);

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

	if (isInstrumentable()) {
    	//RegionPushCDep(region, 0);
		CDepInitRegion(getIndex(level));
		assert(CDepGet(getIndex(level)) == 0ULL);
	}
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
RegionField fillRegionField(UInt64 work, UInt64 cp, CID callSiteId, UInt64 spWork, UInt64 tpWork, UInt64 isDoall, Region* region_info) {
	RegionField field;

    field.work = work;
    field.cp = cp;
	field.callSite = callSiteId;
	field.spWork = spWork;
	field.tpWork = tpWork;
	field.isDoall = isDoall; 
	field.childCnt = region_info->childCount;

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

void _KExitRegion(SID regionId, RegionType regionType) {
	idbgAction(KREM_REGION_EXIT, "## KExitRegion(regionID=%llu,regionType=%u)\n",regionId,regionType);

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
#define DOALL_THRESHOLD	5
	UInt64 isDoall = (cp - region->childMaxCP) < DOALL_THRESHOLD ? 1 : 0;
	if (regionType != RegionLoop)
		isDoall = 0;
	//fprintf(stderr, "isDoall = %d\n", isDoall);

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
		Region* parentRegion = RegionGet(level - 1);
    	parentSid = parentRegion->regionId;
		parentRegion->childrenWork += work;
		parentRegion->childrenCP += cp;
		parentRegion->childCount++;
		if (parentRegion->childMaxCP < cp) 
			parentRegion->childMaxCP = cp;
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

	if (level < getMaxLevel() && sp < 0.999) {
		fprintf(stderr, "sp = %.2f sid=%u work=%llu childrenWork = %llu childrenCP=%lld\n", sp, sid, work,
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
						spWork, tpWork, isDoall, region);
	CRegionLeave(&field);
        
    if (regionType == RegionFunc) { 
		handleFuncRegionExit(); 
	}

    decrementRegionLevel();
	MSG(0, "\n");
}


/*****************************************************************
 * KReduction, KBinary, KBinaryConst
 *****************************************************************/


void* _KReduction(UInt opCost, Reg dest) {
    MSG(3, "KReduction ts[%u] with cost = %d\n", dest, opCost);
    if (!isKremlinOn() || !isInstrumentable())
		return;

    addWork(opCost);
    return NULL;
}

void* _KBinary(UInt opCost, Reg src0, Reg src1, Reg dest) {
    MSG(1, "KBinary ts[%u] = max(ts[%u], ts[%u]) + %u\n", dest, src0, src1, opCost);
	idbgAction(KREM_BINOP,"## _KBinary(opCost=%u,src0=%u,src1=%u,dest=%u)\n",opCost,src0,src1,dest);

    if (!isKremlinOn())
        return NULL;

    addWork(opCost);
	Index depth = getIndexDepth();
	
	Index index;

    for (index = 0; index < getIndexDepth(); index++) {
		// CDep and shadow memory are index based
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		assert(cdt <= getTimetick() - region->start);
        Time ts0 = RShadowGetItem(src0, index);
        Time ts1 = RShadowGetItem(src1, index);

        Time greater0 = (ts0 > ts1) ? ts0 : ts1;
        Time greater1 = (cdt > greater0) ? cdt : greater0;
        Time value = greater1 + opCost;
		RShadowSetItem(value, dest, index);

        MSG(3, "binOp[%u] level %u version %u \n", opCost, i, RegionGetVersion(i));
        MSG(3, " src0 %u src1 %u dest %u\n", src0, src1, dest);
        MSG(3, " ts0 %u ts1 %u cdt %u value %u\n", ts0, ts1, cdt, value);
		//checkTimestamp(region, ts0);
		//checkTimestamp(region, ts1);
		// region info is level based
        RegionUpdateCp(region, value);
    }
	return NULL;
}

void* _KBinaryConst(UInt opCost, Reg src, Reg dest) {
    MSG(1, "KBinaryConst ts[%u] = ts[%u] + %u\n", dest, src, opCost);
	idbgAction(KREM_BINOP,"## _KBinaryConst(opCost=%u,src=%u,dest=%u)\n",opCost,src,dest);
    if (!isKremlinOn())
        return NULL;

    addWork(opCost);
	Table* table = RShadowGetTable();
	Time* base = table->array; 

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		assert(cdt <= getTimetick() - region->start);
        Time ts0 = RShadowGetItem(src, index);
        Time greater1 = (cdt > ts0) ? cdt : ts0;
        Time value = greater1 + opCost;
		RShadowSetItem(value, dest, index);

        MSG(3, "binOpConst[%u] level %u version %u \n", opCost, i, RegionGetVersion(i));
	    MSG(3, " src %u dest %u\n", src, dest);
   	    MSG(3, " ts0 %u cdt %u value %u\n", ts0, cdt, value);
		RegionUpdateCp(region, value);
    }
    return NULL;
}


void* _KAssign(Reg src, Reg dest) {
    MSG(1, "_KAssign ts[%u] <- ts[%u]\n", dest, src);
	//idbgAction(KREM_BINOP,"## _KAssign(opCost=%u,src=%u,dest=%u)\n",opCost,src,dest);

    if (!isKremlinOn())
    	return NULL;

    return _KBinaryConst(0, src, dest);
}

void* _KAssignConst(UInt dest) {
    MSG(1, "_KAssignConst ts[%u]\n", dest);
	idbgAction(KREM_ASSIGN_CONST,"## _KAssignConst(dest=%u)\n",dest);
    if (!isKremlinOn())
        return NULL;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		RShadowSetItem(cdt, dest, index);
        RegionUpdateCp(region, cdt);
    }
    return NULL;
}

void* _KLoad(Addr addr, Reg dest, UInt32 size) {
    MSG(0, "load size %d ts[%u] = ts[0x%x] + %u\n", size, dest, addr, LOAD_COST);
	idbgAction(KREM_LOAD, "## logLoadInst(Addr=0x%x,dest=%u,size=%u)\n",
		addr, dest, size);

	assert(size <= 8);
	checkRegion();
    if (!isKremlinOn())
    	return NULL;

    addWork(LOAD_COST);

	Index index;
	Index depth = getIndexDepth();
	Level minLevel = getLevel(0);
	Time* tArray = MShadowGet(addr, depth, RegionGetVArray(minLevel), size);

    for (index = 0; index < depth; index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		Time ts0 = tArray[index];
        Time greater1 = (cdt > ts0) ? cdt : ts0;
        Time value = greater1 + LOAD_COST;

        MSG(3, "KLoad level %u version %u \n", i, RegionGetVersion(i));
        MSG(3, " addr 0x%x dest %u\n", addr, dest);
        MSG(3, " cdt %u tsAddr %u max %u\n", cdt, ts0, greater1);
		checkTimestamp(index, region, ts0);
        RShadowSetItem(value, dest, index);
        RegionUpdateCp(region, value);
    }

    MSG(3, "load ts[%u] over\n\n");
    return NULL;
}

void* _KLoad1(Addr addr, UInt src1, UInt dest, UInt32 size) {
    MSG(0, "load1 ts[%u] = max(ts[0x%x],ts[%u]) + %u\n", dest, addr, src1, LOAD_COST);
	idbgAction(KREM_LOAD,"## KLoad1(Addr=0x%x,src1=%u,dest=%u,size=%u)\n",addr,src1,dest,size);
    if (!isKremlinOn())
		return NULL;

    addWork(LOAD_COST);

    Level minLevel = getStartLevel();

	Index index;
	Time* tArray = MShadowGet(addr, getIndexDepth(), RegionGetVArray(minLevel), size);

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		Time tsAddr = tArray[index];
		Time tsSrc1 = RShadowGetItem(src1, index);

        Time max1 = (tsAddr > cdt) ? tsAddr : cdt;
        Time max2 = (max1 > tsSrc1) ? max1 : tsSrc1;
		Time value = max2 + LOAD_COST;

        MSG(3, "KLoad1 level %u version %u \n", i, RegionGetVersion(i));
        MSG(3, " addr 0x%x src1 %u dest %u\n", addr, src1, dest);
        MSG(3, " cdt %u tsAddr %u tsSrc1 %u max %u\n", cdt, tsAddr, tsSrc1, max2);
		checkTimestamp(index, region, tsAddr);
        RShadowSetItem(value, dest, index);
        RegionUpdateCp(region, value);
    }

    return NULL;
}

// TODO: will be removed once kremlin-cc is updated with new APIs
void* _KLoad2(Addr src_addr, UInt src1, UInt src2, UInt dest, UInt32 width) { return _KLoad(src_addr,dest, width); }
void* _KLoad3(Addr src_addr, UInt src1, UInt src2, UInt src3, UInt dest, UInt32 width) { return _KLoad(src_addr,dest, width); }
void* _KLoad4(Addr src_addr, UInt src1, UInt src2, UInt src3, UInt src4, UInt dest, UInt32 width) { return _KLoad(src_addr,dest, width); }


void* _KStore(UInt src, Addr dest_addr, UInt32 size) {
	assert(size <= 8);
    MSG(0, "store size %d ts[0x%x] = ts[%u] + %u\n", size, dest_addr, src, STORE_COST);
	idbgAction(KREM_STORE,"## KStore(src=%u,dest_addr=0x%x,size=%u)\n",src,dest_addr,size);
    if (!isKremlinOn())
    	return NULL;


    addWork(STORE_COST);

	Index index;
	Time* tArray = RegionGetTArray();

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time cdt = CDepGet(index);
		Time ts0 = RShadowGetItem(src, index);
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
	MShadowSet(dest_addr, getIndexDepth(), RegionGetVArray(minLevel), tArray, size);
    return NULL;
}


void* _KStoreConst(Addr dest_addr, UInt32 size) {
    MSG(0, "KStoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
	idbgAction(KREM_STORE,"## KStoreConst (dest_addr=0x%x,size=%u)\n",dest_addr,size);
    if (!isKremlinOn())
        return NULL;


    addWork(STORE_COST);

	Index index;
	Time* tArray = RegionGetTArray();

    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Index index = getIndex(i);
		Time cdt = CDepGet(index);
        Time value = cdt + STORE_COST;
		tArray[index] = value;
		RegionUpdateCp(region, value);
    }
	Level minLevel = getLevel(0);
	MShadowSet(dest_addr, getIndexDepth(), RegionGetVArray(minLevel), tArray, size);
    return NULL;
}


// this function is the same as _KAssignConst but helps to quickly
// identify induction variables in the source code
void* _KInduction(UInt dest) {
    MSG(1, "KInduction to %u\n", dest);
    if (!isKremlinOn())
		return NULL;

    return _KAssignConst(dest);
}

/******************************************************************
 * logPhi Functions
 *
 *  for the efficiency, we use several versions with different 
 *  number of incoming dependences
 ******************************************************************/

void* _KPhi1To1(UInt dest, UInt src, UInt cd) {
    MSG(1, "KPhi1To1 ts[%u] = max(ts[%u], ts[%u])\n", dest, src, cd);
	idbgAction(KREM_PHI,"## KPhi1To1 (dest=%u,src=%u,cd=%u)\n",dest,src,cd);
    if (!isKremlinOn())
		return NULL;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Time ts_src = RShadowGetItem(src, index);
		Time ts_cd = RShadowGetItem(cd, index);
        Time max = (ts_src > ts_cd) ? ts_src : ts_cd;
		RShadowSetItem(max, dest, index);
        MSG(3, "KPhi1To1 level %u version %u \n", i, RegionGetVersion(i));
        MSG(3, " src %u cd %u dest %u\n", src, cd, dest);
        MSG(3, " ts_src %u ts_cd %u max %u\n", ts_src, ts_cd, max);
    }
    return NULL;
}

void* _KPhi2To1(UInt dest, UInt src, UInt cd1, UInt cd2) {
    MSG(1, "KPhi2To1 ts[%u] = max(ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2);
	idbgAction(KREM_PHI,"## KPhi2To1 (dest=%u,src=%u,cd1=%u,cd2=%u)\n",dest,src,cd1,cd2);
    if (!isKremlinOn())
    	return NULL;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Time ts_src = RShadowGetItem(src, index);
		Time ts_cd1 = RShadowGetItem(cd1, index);
		Time ts_cd2 = RShadowGetItem(cd2, index);
        Time max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Time max2 = (max1 > ts_cd2) ? max1 : ts_cd2;

		RShadowSetItem(max2, dest, index);

        MSG(2, "KPhi2To1 level %u version %u \n", i, RegionGetVersion(i));
        MSG(2, " src %u cd1 %u cd2 %u dest %u\n", src, cd1, cd2, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u max %u\n", ts_src, ts_cd1, ts_cd2, max2);
    }

    //return entryDest;
    return NULL;
}

void* _KPhi3To1(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3) {
    MSG(1, "KPhi3To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3);
	idbgAction(KREM_PHI,"## KPhi3To1 (dest=%u,src=%u,cd1=%u,cd2=%u,cd3=%u)\n",dest,src,cd1,cd2,cd3);
    if (!isKremlinOn())
    	return NULL;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Time ts_src = RShadowGetItem(src, index);
		Time ts_cd1 = RShadowGetItem(cd1, index);
		Time ts_cd2 = RShadowGetItem(cd2, index);
		Time ts_cd3 = RShadowGetItem(cd3, index);
        Time max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Time max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Time max3 = (max2 > ts_cd3) ? max2 : ts_cd3;

		RShadowSetItem(max3, dest, index);

        MSG(2, "KPhi3To1 level %u version %u \n", i, RegionGetVersion(i));
        MSG(2, " src %u cd1 %u cd2 %u cd3 %u dest %u\n", src, cd1, cd2, cd3, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u max %u\n", ts_src, ts_cd1, ts_cd2, ts_cd3, max3);
    }

    return NULL;
}

void* _KPhi4To1(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    MSG(1, "KPhi4To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest, src, cd1, cd2, cd3, cd4);
	idbgAction(KREM_PHI,"## KPhi4To1 (dest=%u,src=%u,cd1=%u,cd2=%u,cd3=%u,cd4=%u)\n",
		dest,src,cd1,cd2,cd3,cd4);
    if (!isKremlinOn())
    	return NULL;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Time ts_src = RShadowGetItem(src, index);
		Time ts_cd1 = RShadowGetItem(cd1, index);
		Time ts_cd2 = RShadowGetItem(cd2, index);
		Time ts_cd3 = RShadowGetItem(cd3, index);
		Time ts_cd4 = RShadowGetItem(cd4, index);
        Time max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
        Time max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Time max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Time max4 = (max3 > ts_cd4) ? max3 : ts_cd4;

		RShadowSetItem(max4, dest, index);

        MSG(2, "KPhi4To1 level %u version %u \n", i, RegionGetVersion(i));
        MSG(2, " src %u cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", src, cd1, cd2, cd3, cd4, dest);
        MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", 
			ts_src, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
    }

    return NULL;
}

void* _KPhiCond4To1(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
    MSG(1, "KPhi4To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest, dest, cd1, cd2, cd3, cd4);
	idbgAction(KREM_CD_TO_PHI,"## KPhi4To1 (dest=%u,cd1=%u,cd2=%u,cd3=%u,cd4=%u)\n",
		dest,cd1,cd2,cd3,cd4);

    if (!isKremlinOn())
		return NULL;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
        Time ts_dest = RShadowGetItem(dest, index);
		Time ts_cd1 = RShadowGetItem(cd1, index);
		Time ts_cd2 = RShadowGetItem(cd2, index);
		Time ts_cd3 = RShadowGetItem(cd3, index);
		Time ts_cd4 = RShadowGetItem(cd4, index);
        Time max1 = (ts_dest > ts_cd1) ? ts_dest : ts_cd1;
        Time max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
        Time max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
        Time max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
		RShadowSetItem(max4, dest, index);

        MSG(2, "KPhi4To1 level %u version %u \n", i, RegionGetVersion(i));
        MSG(2, " cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", cd1, cd2, cd3, cd4, dest);
        MSG(2, " ts_dest %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", ts_dest, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
    }
    return NULL;
}

#define MAX_ENTRY 10

void* _KPhiAddCond(UInt dest, UInt src) {
    MSG(1, "KPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest, src, dest);
	idbgAction(KREM_CD_TO_PHI,"## KPhiAddCond (dest=%u,src=%u)\n",dest,src);

    if (!isKremlinOn())
    	return;

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
		Level i = getLevel(index);
		Region* region = RegionGet(i);
		Time ts0 = RShadowGetItem(src, index);
		Time ts1 = RShadowGetItem(dest, index);
        Time value = (ts0 > ts1) ? ts0 : ts1;
		RShadowSetItem(value, dest, index);
        RegionUpdateCp(region, value);
        MSG(2, "KPhiAddCond level %u version %u \n", i, RegionGetVersion(i));
        MSG(2, " src %u dest %u\n", src, dest);
        MSG(2, " ts0 %u ts1 %u value %u\n", ts0, ts1, value);
    }
}



/******************************
 * Kremlin Init / Deinit
 *****************************/

#define MSHADOW_BASE	0
#define MSHADOW_STV		1
#define MSHADOW_CACHE	2
#define MSHADOW_DUMMY   3


void MShadowInit() {
	switch(KConfigGetShadowType()) {
		case 0:
			MShadowInitBase();	
			break;
		case 1:
			MShadowInitSTV();	
			break;
		case 2:
			MShadowInitSkadu();	
			break;
		case 3:
			MShadowInitDummy();
			break;
	}
}

void MShadowDeinit() {
	switch(KConfigGetShadowType()) {
		case 0:
			MShadowDeinitBase();	
			break;
		case 1:
			MShadowDeinitSTV();	
			break;
		case 2:
			MShadowDeinitSkadu();	
			break;
		case 3:
			MShadowDeinitDummy();
			break;
	}
}

static UInt hasInitialized = 0;

#define REGION_INIT_SIZE	64

static Bool kremlinInit() {
	DebugInit("kremlin.log");
    if(hasInitialized++) {
        MSG(0, "kremlinInit skipped\n");
        return FALSE;
    }
	initLevels();

    MSG(0, "Profile Level = (%d, %d), Index Size = %d\n", 
        getMinLevel(), getMaxLevel(), getIndexSize());
    MSG(0, "kremlinInit running....");
	if(getKremlinDebugFlag()) { 
		fprintf(stderr,"[kremlin] debugging enabled at level %d\n", getKremlinDebugLevel()); 
	}

#if 0
    InvokeRecordsCreate(&invokeRecords);
#endif
	ArgFifoInit();
	CDepInit();
	CRegionInit();
	RShadowInit(getIndexSize());

	MShadowInit(KConfigGetCacheSize());
	RegionInit(REGION_INIT_SIZE);
   	_KTurnOn();
    return TRUE;
}

static Bool kremlinDeinit() {
    if(--hasInitialized) {
        MSG(0, "kremlinDeinit skipped\n");
        return FALSE;
    }

	fprintf(stderr,"[kremlin] max active level = %d\n", 
		getMaxActiveLevel());	
	_KTurnOff();
	CRegionDeinit("kremlin.bin");
	RShadowDeinit();
	MShadowDeinit();
	ArgFifoDeinit();
	CDepDeinit();
	RegionDeinit();
    //MemMapAllocatorDelete(&memPool);

	DebugDeinit();
    return TRUE;
}

void _KInit() {
    kremlinInit();
}

void _KDeinit() {
    kremlinDeinit();
}

void _KPrintData() {}


/**************************************************************************
 * Start of Non-Essential APIs
 *************************************************************************/

/***********************************************
 * Library Call
 * DJ: will be optimized later..
 ************************************************/


// use estimated cost for a callee function we cannot instrument
// TODO: implement new shadow mem interface
void* _KCallLib(UInt cost, UInt dest, UInt num_in, ...) {
	idbgAction(KREM_CD_TO_PHI,"## KCallLib(cost=%u,dest=%u,num_in=%u,...)\n",cost,dest,num_in);

#if 0
    if (!isKremlinOn())
        return NULL;

    MSG(1, "logLibraryCall to ts[%u] with cost %u\n", dest, cost);
    addWork(cost);

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
    return NULL;
#endif
    
}

/***********************************************
 * Dynamic Memory Allocation / Deallocation
 * DJ: will be optimized later..
 ************************************************/


// FIXME: support 64 bit address
void _KMalloc(Addr addr, size_t size, UInt dest) {
#if 0
    if (!isKremlinOn()) return;
    
    MSG(1, "logMalloc addr=0x%x size=%llu\n", addr, (UInt64)size);

    // Don't do anything if malloc returned NULL
    if(!addr) { return; }

    createMEntry(addr,size);
#endif
}

// TODO: implement for new shadow mem interface
void _KFree(Addr addr) {
#if 0
    if (!isKremlinOn()) return;

    MSG(1, "logFree addr=0x%x\n", addr);

    // Calls to free with NULL just return.
    if(addr == NULL) return;

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
}

// TODO: more efficient implementation (if old_addr = new_addr)
// XXX: This is wrong. Values in the realloc'd location should still have the
// same timestamp.
void _KRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest) {
#if 0
    if (!isKremlinOn())
        return;

    MSG(1, "logRealloc old_addr=0x%x new_addr=0x%x size=%llu\n", old_addr, new_addr, (UInt64)size);
    logFree(old_addr);
    logMalloc(new_addr,size,dest);
#endif
}

/***********************************************
 * Kremlin Interactive Debugger Functions 
 ************************************************/
void printActiveRegionStack() {
	fprintf(stdout,"Current Regions:\n");

	int i;
	Level level = getCurrentLevel();

	for(i = 0; i <= level; ++i) {
		Region* region = RegionGet(i);
		fprintf(stdout,"#%d: ",i);
		if(region->regionType == RegionFunc) {
			fprintf(stdout,"type=FUNC ");
		}
		else if(region->regionType == RegionLoop) {
			fprintf(stdout,"type=LOOP ");
		}
		else if(region->regionType == RegionLoopBody) {
			fprintf(stdout,"type=BODY ");
		}
		else {
			fprintf(stdout,"type=ILLEGAL ");
		}

    	UInt64 work = getTimetick() - region->start;
		fprintf(stdout,"SID=%llu, WORK'=%llu, CP=%llu\n",region->regionId,work,region->cp);
	}
}

void printControlDepTimes() {
	fprintf(stdout,"Control Dependency Times:\n");
	Index index;

    for (index = 0; index < getIndexDepth(); index++) {
		Time cdt = CDepGet(index);
		fprintf(stdout,"\t#%u: %llu\n",index,cdt);
	}
}

void printRegisterTimes(Reg reg) {
	fprintf(stdout,"Timestamps for reg[%u]:\n",reg);

	Index index;
    for (index = 0; index < getIndexDepth(); index++) {
        Time ts = RShadowGetItem(reg, index);
		fprintf(stdout,"\t#%u: %llu\n",index,ts);
	}
}

void printMemoryTimes(Addr addr, Index size) {
	fprintf(stdout,"Timestamps for Mem[%x]:\n",addr);
	Index index;
	Index depth = getIndexDepth();
	Level minLevel = getLevel(0);
	Time* tArray = MShadowGet(addr, depth, RegionGetVArray(minLevel), size);

    for (index = 0; index < depth; index++) {
		Time ts = tArray[index];
		fprintf(stdout,"\t#%u: %llu\n",index,ts);
	}
}

#if 0
/***********************************************
 * DJ: not sure what these are for 
 ************************************************/

void* logInsertValue(UInt src, UInt dest) {
	assert(0);
    //printf("Warning: logInsertValue not correctly implemented\n");

#ifdef KREMLIN_DEBUG
	if(__kremlin_idbg) {
		if(__kremlin_idbg_run_state == Waiting) {
    		fprintf(stdout, "## logInsertValue(src=%u,dest=%u)\n\t",src,dest);
		}
	}
#endif

    return _KAssign(src, dest);
}

void* logInsertValueConst(UInt dest) {
	assert(0);
    //printf("Warning: logInsertValueConst not correctly implemented\n");

#ifdef KREMLIN_DEBUG
	if(__kremlin_idbg) {
		if(__kremlin_idbg_run_state == Waiting) {
    		fprintf(stdout, "## logInsertValueConst(dest=%u)\n\t",dest);
		}
	}
#endif

    return _KAssignConst(dest);
}
#endif

#if 0
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


void _KPrepInvoke(UInt64 id) {
    if(!isKremlinOn())
        return;

    MSG(1, "prepareInvoke(%llu) - saved at %lld\n", id, (UInt64)getCurrentLevel());
   
    InvokeRecord* currentRecord = InvokeRecordsPush(invokeRecords);
    currentRecord->id = id;
    currentRecord->stackHeight = getCurrentLevel();
}

void _KInvokeOkay(UInt64 id) {
    if(!isKremlinOn())
        return;

    if(!InvokeRecordsEmpty(invokeRecords) && InvokeRecordsLast(invokeRecords)->id == id) {
        MSG(1, "invokeOkay(%u)\n", id);
        InvokeRecordsPop(invokeRecords);
    } else
        MSG(1, "invokeOkay(%u) ignored\n", id);
}

void _KInvokeThrew(UInt64 id)
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



#endif 

#include "interface.h"

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
#include "config.h"
#include "CRegion.h"

#include "MShadow.h"
#include "MShadowDummy.h"
#include "MShadowBase.h"
#include "MShadowSTV.h"
#include "MShadowSkadu.h"

#include "RShadow.h"
#include "RShadow.cpp" // WHY?
#include "PoolAllocator.hpp"

#include "ProgramRegion.hpp"
#include "FuncContext.hpp"

#include <vector>
#include <iostream>

//#include "idbg.h"
#define LOAD_COST           4
#define STORE_COST          1
#define MALLOC_COST         100
#define FREE_COST           10

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

#define MAX(a, b)   (((a) > (b)) ? (a) : (b))
#define MAX3(a, b, c)   (((MAX(a,b)) > (c)) ? (MAX(a,b)) : (b))
#define MAX4(a, b, c, d)   ( MAX(MAX(a,b),MAX(c,d)) )

#define isKremlinOn()		(kremlinOn == 1)

static CID lastCallSiteId; // TODO: member of KremlinProfiler?

// TODO: make this a member of KremlinProfiler?
static std::vector<FuncContext*, MPoolLib::PoolAllocator<FuncContext*> > funcContexts; // A vector used to represent the call stack.

class KremlinProfiler {
private:
	Time curr_time; // the current time of the profiler (virtual)
	Level curr_level; // current level 
	Level min_level; // minimum level to instrument
	Level max_level; // maximum level to instrument
	Level max_active_level; // max level we have seen thusfar

	Index curr_num_instrumented_levels; // number of regions currently instrumented
	bool instrument_curr_level; // whether we should instrument the current level


	static const unsigned int FUNC_ARG_QUEUE_SIZE = 64;
	std::vector<Reg> function_arg_queue;
	unsigned int arg_queue_read_index;
	unsigned int arg_queue_write_index;

	void updateCurrLevelInstrumentableStatus() {
		if (curr_level >= min_level && curr_level <= max_level)
			instrument_curr_level = true;
		else 
			instrument_curr_level = false;
	}

public:
	KremlinProfiler(Level min, Level max) {
		this->curr_time = 0;
		this->curr_level = -1;
		this->min_level = min;
		this->max_level = max;
		this->max_active_level = 0;
		this->curr_num_instrumented_levels = 0;
		this->instrument_curr_level = false;
	}

	~KremlinProfiler() {}

	int getCurrentTime() { return this->curr_time; }
	int getCurrentLevel() { return this->curr_level; }
	Level getCurrentLevelIndex() { return curr_level - min_level; }
	int getMinLevel() { return this->min_level; }
	int getMaxLevel() { return this->max_level; }
	int getMaxActiveLevel() { return this->max_active_level; }
	bool shouldInstrumentCurrLevel() { return instrument_curr_level; }

	int getArraySize() { return max_level - min_level + 1; }
	int getCurrNumInstrumentedLevels() { return curr_num_instrumented_levels; }

	Level getLevelForIndex(Index index) { return min_level + index; }

	void increaseTime(UInt32 amount) { curr_time += amount; } // XXX: UInt32 -> Time?

	void incrementLevel() { 
		++curr_level;
		updateCurrLevelInstrumentableStatus();
		updateCurrNumInstrumentedLevels();

		if (curr_level > max_active_level)
			max_active_level = curr_level;
	}
	void decrementLevel() { 
		--curr_level;
		updateCurrLevelInstrumentableStatus();
		updateCurrNumInstrumentedLevels();
	}

	/*! \brief Update number of levels being instrumented based on our current
	 * level.
	 */
	void updateCurrNumInstrumentedLevels() {
		if (!KConfigLimitLevel()) {
			curr_num_instrumented_levels = curr_level + 1;
		}

		else if (curr_level < min_level) {
			curr_num_instrumented_levels = 0;
		}

		else {
			curr_num_instrumented_levels = MIN(max_level, curr_level) - min_level + 1;	
		}
	}

	/**
	 * Pushes new context onto function context stack.
	 */
	void addFunctionToStack(CID cid) {
		FuncContext* fc = (FuncContext*) MemPoolAllocSmall(sizeof(FuncContext));
		assert(fc);

		fc->init(cid);
		funcContexts.push_back(fc);

		MSG(3, "addFunctionToStack at 0x%x CID 0x%x\n", fc, cid);
	}

	/**
	 * Removes context at the top of the function context stack.
	 */
	void removeFunctionFromStack() {
		FuncContext* fc = funcContexts.back();
		funcContexts.pop_back();
		
		assert(fc);
		MSG(3, "removeFunctionFromStack at 0x%x CID 0x%x\n", fc, fc->getCallSiteID());

		assert(_regionFuncCnt == _setupTableCnt);
		assert(_requireSetupTable == 0);

		if (fc->table != NULL) Table::destroy(fc->table);

		MemPoolFreeSmall(fc, sizeof(FuncContext));
	}

	FuncContext* getCurrentFunction() {
		if (funcContexts.empty()) {
			MSG(3, "getCurrentFunction  NULL\n");
			return NULL;
		}

		FuncContext* func = funcContexts.back();

		MSG(3, "getCurrentFunction  0x%x CID 0x%x\n", func, func->getCallSiteID());
		func->sanityCheck();
		return func;
	}

	FuncContext* getCallingFunction() {
		if (funcContexts.size() == 1) {
			MSG(3, "getCallingFunction  No Caller Context\n");
			return NULL;
		}
		FuncContext* func = funcContexts[funcContexts.size()-2];

		MSG(3, "getCallingFunction  0x%x CID 0x%x\n", func, func->getCallSiteID());
		func->sanityCheck();
		return func;
	}

	void initFunctionArgQueue() {
		function_arg_queue.resize(KremlinProfiler::FUNC_ARG_QUEUE_SIZE, -1);
		clearFunctionArgQueue();
	}

	void deinitFunctionArgQueue() {}

	void functionArgQueuePushBack(Reg src) {
		function_arg_queue[arg_queue_write_index++] = src;
		assert(arg_queue_write_index < function_arg_queue.size());
	}

	Reg functionArgQueuePopFront() {
		assert(arg_queue_read_index < arg_queue_write_index);
		return function_arg_queue[arg_queue_read_index++];
	}

	void clearFunctionArgQueue() {
		arg_queue_read_index = 0;
		arg_queue_write_index = 0;
	}
};

static KremlinProfiler *profiler;

// XXX: hacky... badness!
Level getMaxActiveLevel() {
	return profiler->getMaxActiveLevel();
}

// BEGIN TODO: make these not global
static bool 	kremlinOn = false;
static UInt64	loadCnt = 0llu;
static UInt64	storeCnt = 0llu;


static UInt64 _regionFuncCnt;
static UInt64 _setupTableCnt;
static int _requireSetupTable;

MShadow *mem_shadow = NULL;
// END TODO: make these not global

/*************************************************************
 * Index Management
 * Index represents the offset in multi-value shadow memory
 *************************************************************/

/*************************************************************
 * Global Timestamp Management
 *************************************************************/

void _KWork(UInt32 work) {
	profiler->increaseTime(work);
}

/*************************************************************
 * Arg Management
 *
 * Function Arg Transfer Sequence
 * 1) caller calls "_KPrepCall" to reset function_arg_queue
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

// XXX: moved these to KremlinProfiler


/*****************************************************************
 * Function Context Management
 *****************************************************************/

/*****************************************************************
 * Region Management
 *****************************************************************/

std::vector<ProgramRegion*, MPoolLib::PoolAllocator<ProgramRegion*> > ProgramRegion::program_regions;
unsigned int ProgramRegion::arraySize = 512;
Version* ProgramRegion::vArray = NULL;
Time* ProgramRegion::tArray = NULL;
Version ProgramRegion::nextVersion = 0;



static inline void checkTimestamp(int index, ProgramRegion* region, Timestamp value) {
#ifndef NDEBUG
	if (value > profiler->getCurrentTime() - region->start) {
		fprintf(stderr, "index = %d, value = %lld, current time = %lld, region start = %lld\n", 
		index, value, profiler->getCurrentTime(), region->start);
		assert(0);
	}
#endif
}

static inline void RegionUpdateCp(ProgramRegion* region, Timestamp value) {
	region->cp = MAX(value, region->cp);
	MSG(3, "RegionUpdateCp : value = %llu\n", region->cp);	
	region->sanityCheck();
#ifndef NDEBUG
	//assert(value <= profiler->getCurrentTime() - region->start);
	if (value > profiler->getCurrentTime() - region->start) {
		fprintf(stderr, "value = %lld, current time = %lld, region start = %lld\n", 
		value, profiler->getCurrentTime(), region->start);
		assert(0);
	}
#endif
}


void checkRegion() {
#if 0
	int bug = 0;
	for (unsigned i=0; i < regionInfo.size(); ++i) {
		ProgramRegion* ret = regionInfo[i];
		if (ret->code != 0xDEADBEEF) { // XXX: won't work now
			MSG(0, "ProgramRegion Error at index %d\n", i);	
			assert(0);
			assert(ret->code == 0xDEADBEEF);
		}
	}
	if (bug > 0)
		assert(0);
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
	cTable = Table::create(CDEP_ROW, CDEP_COL);
}

inline void CDepDeinit() {
	Table::destroy(cTable);
}

inline void CDepInitRegion(Index index) {
	assert(cTable != NULL);
	MSG(3, "CDepInitRegion ReadPtr = %d, Index = %d\n", cTableReadPtr, index);
	cTable->setValue(0ULL, cTableReadPtr, index);
	cTableCurrentBase = cTable->getElementAddr(cTableReadPtr, 0);
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
	int indexSize = profiler->getCurrNumInstrumentedLevels();

// TODO: rarely, ctable could require resizing..not implemented yet
	if (cTableReadPtr == cTable->getRow()) {
		fprintf(stderr, "CDep Table requires entry resizing..\n");
		assert(0);	
	}

	if (cTable->getCol() < indexSize) {
		fprintf(stderr, "CDep Table requires index resizing..\n");
		assert(0);	
	}

	Table* ltable = RShadowGetTable();
	//assert(lTable->col >= indexSize);
	//assert(cTable->col >= indexSize);

	lTable->copyToDest(cTable, cTableReadPtr, cond, 0, indexSize);
	cTableCurrentBase = cTable->getElementAddr(cTableReadPtr, 0);
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
	cTableCurrentBase = cTable->getElementAddr(cTableReadPtr, 0);
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
	idbgAction(KREM_PREP_CALL, "## _KPrepCall(callSiteId=%llu,calledRegionId=%llu)\n",callSiteId,calledRegionId);
    if (!isKremlinOn()) { 
		return; 
	}

    // Clear off any argument timestamps that have been left here before the
    // call. These are left on the deque because library calls never take
    // theirs off. 
    profiler->clearFunctionArgQueue();
	lastCallSiteId = callSiteId;
}

void _KEnqArg(Reg src) {
    MSG(1, "Enque Arg Reg [%u]\n", src);
	idbgAction(KREM_LINK_ARG,"## _KEnqArg(src=%u)\n",src);
    if (!isKremlinOn())
        return;
	profiler->functionArgQueuePushBack(src);
}

#define DUMMY_ARG		-1
void _KEnqArgConst() {
    MSG(1, "Enque Const Arg\n");
	idbgAction(KREM_LINK_ARG,"## _KEnqArgConst()\n");
    if (!isKremlinOn())
        return;

	profiler->functionArgQueuePushBack(DUMMY_ARG); // dummy arg
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
	idbgAction(KREM_UNLINK_ARG,"## _KDeqArg(dest=%u)\n",dest);
    if (!isKremlinOn())
        return;

	Reg src = profiler->functionArgQueuePopFront();
	// copy parent's src timestamp into the currenf function's dest reg
	if (src != DUMMY_ARG && profiler->getCurrNumInstrumentedLevels() > 0) {
		FuncContext* caller = profiler->getCallingFunction();
		FuncContext* callee = profiler->getCurrentFunction();
		Table* callerT = caller->getTable();
		Table* calleeT = callee->getTable();

		// decrement one as the current level should not be copied
		int indexSize = profiler->getCurrNumInstrumentedLevels() - 1;
		assert(profiler->getCurrentLevel() >= 1);
		callerT->copyToDest(calleeT, dest, src, 0, indexSize);
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
 *  - col: profiler->getCurrentLevel() + 1 + maxNestLevel
 *		maxNestLevel represents the max depth of a region that can use 
 *		the RTable. 
 */
void _KPrepRTable(UInt maxVregNum, UInt maxNestLevel) {
	int tableHeight = maxVregNum;
	int tableWidth = profiler->getCurrentLevel() + maxNestLevel + 1;
    MSG(1, "KPrep RShadow Table row=%d, col=%d (curLevel=%d, maxNestLevel=%d)\n",
		 tableHeight, tableWidth, profiler->getCurrentLevel(), maxNestLevel);
	idbgAction(KREM_PREP_REG_TABLE,"## _KPrepRTable(maxVregNum=%u,maxNestLevel=%u)\n",maxVregNum,maxNestLevel);

    if (!isKremlinOn()) {
		 return; 
	}

    assert(_requireSetupTable == 1);
    Table* table = Table::create(tableHeight, tableWidth);
    FuncContext* funcHead = profiler->getCurrentFunction();
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

	FuncContext* caller = profiler->getCurrentFunction();
	caller->setReturnRegister(dest);
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

    FuncContext* callee = profiler->getCurrentFunction();
    FuncContext* caller = profiler->getCallingFunction();

	// main function does not have a return point
	if (caller == NULL)
		return;

	Reg ret = caller->getReturnRegister();
	assert(ret >= 0);

	// current level time does not need to be copied
	int indexSize = profiler->getCurrNumInstrumentedLevels() - 1;
	if (indexSize > 0)
		callee->table->copyToDest(caller->table, ret, src, 0, indexSize);
	
    MSG(1, "end write return value 0x%x\n", profiler->getCurrentFunction());
}

void _KReturnConst(void) {
    MSG(1, "_KReturnConst\n");
	idbgAction(KREM_FUNC_RETURN,"## _KReturnConst()\n");
    if (!isKremlinOn())
        return;

    // Assert there is a function context before the top.
	FuncContext* caller = profiler->getCallingFunction();

	// main function does not have a return point
	if (caller == NULL)
		return;

	Index index;
    for (index = 0; index < caller->table->col; index++) {
		Time cdt = CDepGet(index);
		caller->table->setValue(cdt, caller->getReturnRegister(), index);
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
 * KEnterRegion / KExitRegion
 *****************************************************************/

void _KEnterRegion(SID regionId, RegionType regionType) {
	iDebugHandlerRegionEntry(regionId);
	idbgAction(KREM_REGION_ENTRY,"## KEnterRegion(regionID=%llu,regionType=%u)\n",regionId,regionType);

    if (!isKremlinOn()) { 
		return; 
	}

    profiler->incrementLevel();
    Level level = profiler->getCurrentLevel();
	if (level == ProgramRegion::getNumRegions()) {
		ProgramRegion::doubleNumRegions();
	}
	
	ProgramRegion* region = ProgramRegion::getRegionAtLevel(level);
	region->init(regionId, regionType, level, profiler->getCurrentTime());

	MSG(0, "\n");
	MSG(0, "[+++] region [type %u, level %d, sid 0x%llx] start: %llu\n",
        region->regionType, regionType, level, region->regionId, profiler->getCurrentTime());
    incIndentTab(); // only affects debug printing

	// func region allocates a new RShadow Table.
	// for other region types, it needs to "clean" previous region's timestamps
    if(regionType == RegionFunc) {
		_regionFuncCnt++;
        profiler->addFunctionToStack(lastCallSiteId);
        _requireSetupTable = 1;

    } else {
		if (profiler->shouldInstrumentCurrLevel())
			RShadowRestartIndex(profiler->getCurrentLevelIndex());
	}

    FuncContext* funcHead = profiler->getCurrentFunction();
	CID callSiteId = (funcHead == NULL) ? 0x0 : funcHead->getCallSiteID();
	CRegionEnter(regionId, callSiteId, regionType);

	if (profiler->shouldInstrumentCurrLevel()) {
    	//RegionPushCDep(region, 0);
		CDepInitRegion(profiler->getCurrentLevelIndex());
		assert(CDepGet(profiler->getCurrentLevelIndex()) == 0ULL);
	}
	MSG(0, "\n");
}

/**
 * Does the clean up work when exiting a function region.
 */
static void handleFuncRegionExit() {
	profiler->removeFunctionFromStack();

	// root function
	if (funcContexts.empty()) {
		assert(profiler->getCurrentLevel() == 0); 
		return;
	}

	FuncContext* funcHead = profiler->getCurrentFunction();
	assert(funcHead != NULL);
	RShadowActivateTable(funcHead->table); 
}


/**
 * Creates RegionField and fills it based on inputs.
 */
RegionField fillRegionField(UInt64 work, UInt64 cp, CID callSiteId, UInt64 spWork, UInt64 isDoall, ProgramRegion* region_info) {
	RegionField field;

    field.work = work;
    field.cp = cp;
	field.callSite = callSiteId;
	field.spWork = spWork;
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

    Level level = profiler->getCurrentLevel();
	ProgramRegion* region = ProgramRegion::getRegionAtLevel(level);
    SID sid = regionId;
	SID parentSid = 0;
    UInt64 work = profiler->getCurrentTime() - region->start;
	decIndentTab(); // applies only to debug printing
	MSG(0, "\n");
    MSG(0, "[---] region [type %u, level %u, sid 0x%llx] time %llu cp %llu work %llu\n",
        regionType, level, regionId, profiler->getCurrentTime(), region->cp, work);

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
	if (level < profiler->getMaxLevel() && level >= profiler->getMinLevel()) {
		assert(work >= cp);
		assert(work >= region->childrenWork);
	}
#endif

	// Only update parent region's childrenWork and childrenCP 
	// when we are logging the parent
	// If level is higher than max,
	// it will not reach here - 
	// so no need to compare with max level.

	if (level > profiler->getMinLevel()) {
		ProgramRegion* parentRegion = ProgramRegion::getRegionAtLevel(level - 1);
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
    if (profiler->shouldInstrumentCurrLevel() && cp == 0 && work > 0) {
        fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
        fprintf(stderr, "region [type: %u, level: %u, sid: %llu] parent [%llu] cp %llu work %llu\n",
            regionType, level, regionId,  parentSid,  region->cp, work);
        assert(0);
    }

	if (level < profiler->getMaxLevel() && sp < 0.999) {
		fprintf(stderr, "sp = %.2f sid=%u work=%llu childrenWork = %llu childrenCP=%lld cp=%lld\n", sp, sid, work,
			region->childrenWork, region->childrenCP, region->cp);
		assert(0);
	}
#endif

	UInt64 spWork = (UInt64)((double)work / sp);

	// due to floating point variables,
	// spWork can be larger than work
	if (spWork > work) { spWork = work; }

	CID cid = profiler->getCurrentFunction()->getCallSiteID();
    RegionField field = fillRegionField(work, cp, cid, 
						spWork, isDoall, region);
	CRegionExit(&field);
        
    if (regionType == RegionFunc) { 
		handleFuncRegionExit(); 
	}

    profiler->decrementLevel();
	MSG(0, "\n");
}

void _KLandingPad(SID regionId, RegionType regionType) {
	idbgAction(KREM_REGION_EXIT, "## KLandingPad(regionID=%llu,regionType=%u)\n",regionId,regionType);

    if (!isKremlinOn()) return;

	SID sid = 0;

	// find deepest level with region id that matches parameter regionId
	Level end_level = profiler->getCurrentLevel()+1;
	for (unsigned i = profiler->getCurrentLevel(); i >= 0; --i) {
		if (ProgramRegion::getRegionAtLevel(i)->regionId == regionId) {
			end_level = i;
			break;
		}
	}
	assert(end_level != profiler->getCurrentLevel()+1);
	
	while (profiler->getCurrentLevel() > end_level) {
		Level level = profiler->getCurrentLevel();
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(level);

		sid = region->regionId;
		UInt64 work = profiler->getCurrentTime() - region->start;
		decIndentTab(); // applies only to debug printing
		MSG(0, "\n");
		MSG(0, "[!---] region [type %u, level %u, sid 0x%llx] time %llu cp %llu work %llu\n",
			region->regionType, level, sid, profiler->getCurrentTime(), region->cp, work);

		UInt64 cp = region->cp;
#define DOALL_THRESHOLD	5
		UInt64 isDoall = (cp - region->childMaxCP) < DOALL_THRESHOLD ? 1 : 0;
		if (region->regionType != RegionLoop)
			isDoall = 0;
		//fprintf(stderr, "isDoall = %d\n", isDoall);

#ifdef KREMLIN_DEBUG
		if (work < cp) {
			fprintf(stderr, "work = %llu\n", work);
			fprintf(stderr, "cp = %llu\n", cp);
			assert(0);
		}
		if (level < profiler->getMaxLevel() && level >= profiler->getMinLevel()) {
			assert(work >= cp);
			assert(work >= region->childrenWork);
		}
#endif

		// Only update parent region's childrenWork and childrenCP 
		// when we are logging the parent
		// If level is higher than max,
		// it will not reach here - 
		// so no need to compare with max level.

		SID parentSid = 0;
		if (level > profiler->getMinLevel()) {
			ProgramRegion* parentRegion = ProgramRegion::getRegionAtLevel(level - 1);
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
		if (profiler->shouldInstrumentCurrLevel() && cp == 0 && work > 0) {
			fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
			fprintf(stderr, "region [type: %u, level: %u, sid: %llu] parent [%llu] cp %llu work %llu\n",
				regionType, level, regionId,  parentSid,  region->cp, work);
			assert(0);
		}

		if (level < profiler->getMaxLevel() && sp < 0.999) {
			fprintf(stderr, "sp = %.2f sid=%u work=%llu childrenWork = %llu childrenCP=%lld cp=%lld\n", sp, sid, work,
				region->childrenWork, region->childrenCP, region->cp);
			assert(0);
		}
#endif

		UInt64 spWork = (UInt64)((double)work / sp);

		// due to floating point variables,
		// spWork can be larger than work
		if (spWork > work) { spWork = work; }

		CID cid = profiler->getCurrentFunction()->getCallSiteID();
		RegionField field = fillRegionField(work, cp, cid, 
							spWork, isDoall, region);
		CRegionExit(&field);
			
		if (region->regionType == RegionFunc) { 
			handleFuncRegionExit(); 
		}

		profiler->decrementLevel();
		MSG(0, "\n");
	}
}


/*****************************************************************
 * KInduction, KReduction, KTimestamp, KAssignConst
 *****************************************************************/

void _KAssignConst(UInt dest_reg) {
    MSG(1, "_KAssignConst ts[%u]\n", dest_reg);
	idbgAction(KREM_ASSIGN_CONST,"## _KAssignConst(dest_reg=%u)\n",dest_reg);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time control_dep_time = CDepGet(index);
		RShadowSetItem(control_dep_time, dest_reg, index);
        RegionUpdateCp(region, control_dep_time);
    }
}

// This function is mainly to help identify induction variables in the source
// code.
void _KInduction(UInt dest_reg) {
    MSG(1, "KInduction to %u\n", dest_reg);
	idbgAction(KREM_INDUCTION,"## _KInduction(dest_reg=%u)\n",dest_reg);

    if (!isKremlinOn()) return;

	_KAssignConst(dest_reg);
}

void _KReduction(UInt op_cost, Reg dest_reg) {
    MSG(3, "KReduction ts[%u] with cost = %d\n", dest_reg, op_cost);
	idbgAction(KREM_REDUCTION, "## KReduction(op_cost=%u,dest_reg=%u)\n",op_cost,dest_reg);

    if (!isKremlinOn() || !profiler->shouldInstrumentCurrLevel()) return;
}

void _KTimestamp(UInt32 dest_reg, UInt32 num_srcs, ...) {
    MSG(1, "KTimestamp ts[%u] = (0..%u) \n", dest_reg,num_srcs);
	idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,num_srcs=%u,...)\n",dest_reg,num_srcs);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time cdt = CDepGet(index);
		assert(cdt <= profiler->getCurrentTime() - region->start);

		va_list args;
		va_start(args,num_srcs);

		Time curr_max = cdt;

        MSG(3, "kTime level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " dest_reg %u\t", dest_reg);

		int arg_idx;
		for(arg_idx = 0; arg_idx < num_srcs; ++arg_idx) {
			UInt32 src_reg = va_arg(args,UInt32);
			UInt32 src_offset = va_arg(args,UInt32);

        	Time ts_calc = RShadowGetItem(src_reg, index) + src_offset;

			curr_max = MAX(curr_max,ts_calc);

        	MSG(3, "  src_reg%u %u | src_offset%u %u | ts_calc %u\n", arg_idx, src_reg, arg_idx, src_offset, ts_calc);
		}

        MSG(3, " cdt %u | curr_max %u\n", cdt, curr_max);

		RShadowSetItem(curr_max, dest_reg, index);

        RegionUpdateCp(region, curr_max);
    }
}

// XXX: not 100% sure this is the correct functionality
void _KTimestamp0(UInt32 dest_reg) {
    MSG(1, "KTimestamp0 to %u\n", dest_reg);
	idbgAction(KREM_TS,"## _KTimestamp0(dest_reg=%u)\n",dest_reg);
    if (!isKremlinOn()) return;

	_KAssignConst(dest_reg);
}

void _KTimestamp1(UInt32 dest_reg, UInt32 src_reg, UInt32 src_offset) {
    MSG(3, "KTimestamp1 ts[%u] = ts[%u] + %u\n", dest_reg, src_reg, src_offset);
	idbgAction(KREM_TS,"## _KTimestamp1(dest_reg=%u,src_reg=%u,src_offset=%u)\n",dest_reg,src_reg,src_offset);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);

		Time control_dep_time = CDepGet(index);
        Time src_dep_time = RShadowGetItem(src_reg, index) + src_offset;
		assert(control_dep_time <= profiler->getCurrentTime() - region->start);

        Time dest_time = MAX(control_dep_time,src_dep_time);
		RShadowSetItem(dest_time, dest_reg, index);

        MSG(3, "kTime1 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_reg %u | src_offset %u | dest_reg %u\n", src_reg, src_offset, dest_reg);
        MSG(3, " src_dep_time %u | control_dep_time %u | dest_time %u\n", src_dep_time, control_dep_time, dest_time);
        RegionUpdateCp(region, dest_time);
    }
}

void _KTimestamp2(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset) {
    MSG(3, "KTimestamp2 ts[%u] = max(ts[%u] + %u,ts[%u] + %u)\n", dest_reg, src1_reg, src1_offset, src2_reg, src2_offset);
	idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time control_dep_time = CDepGet(index);
		assert(control_dep_time <= profiler->getCurrentTime() - region->start);

        Time src1_dep_time = RShadowGetItem(src1_reg, index) + src1_offset;
        Time src2_dep_time = RShadowGetItem(src2_reg, index) + src2_offset;

        Time max_tmp = MAX(src1_dep_time,src2_dep_time);
        Time dest_time = MAX(control_dep_time,max_tmp);

		RShadowSetItem(dest_time, dest_reg, index);

        MSG(3, "kTime2 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src1_reg %u | src1_offset %u | src2_reg %u | src2_offset %u | dest_reg %u\n", src1_reg, src1_offset, src2_reg, src2_offset, dest_reg);
        MSG(3, " src1_dep_time %u | src2_dep_time %u | control_dep_time %u | dest_time %u\n", src1_dep_time, src2_dep_time, control_dep_time, dest_time);
        RegionUpdateCp(region, dest_time);
    }
}

void _KTimestamp3(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset) {
    MSG(3, "KTimestamp3 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time control_dep_time = CDepGet(index);
		assert(control_dep_time <= profiler->getCurrentTime() - region->start);

        Time src1_dep_time = RShadowGetItem(src1_reg, index) + src1_offset;
        Time src2_dep_time = RShadowGetItem(src2_reg, index) + src2_offset;
        Time src3_dep_time = RShadowGetItem(src3_reg, index) + src3_offset;

        Time dest_time = MAX4(src1_dep_time,src2_dep_time,src3_dep_time,control_dep_time);

		RShadowSetItem(dest_time, dest_reg, index);
        RegionUpdateCp(region, dest_time);

        MSG(3, "kTime3 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src1_reg %u | src1_offset %u",src1_reg,src1_offset);
		MSG(3, " | src2_reg %u | src2_offset %u",src2_reg,src3_offset);
		MSG(3, " | src3_reg %u | src3_offset %u",src3_reg,src3_offset);
		MSG(3, "dest_reg %u\n", dest_reg);
        MSG(3, " src1_dep_time %u",src1_dep_time);
        MSG(3, " | src2_dep_time %u",src2_dep_time);
        MSG(3, " | src3_dep_time %u",src3_dep_time);
        MSG(3, " | dest_time %u\n",dest_time);
    }
}

void _KTimestamp4(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32 src4_reg, UInt32 src4_offset) {
    MSG(3, "KTimestamp4 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u,"
	"ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset,src4_reg,src4_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time control_dep_time = CDepGet(index);
		assert(control_dep_time <= profiler->getCurrentTime() - region->start);

        Time src1_dep_time = RShadowGetItem(src1_reg, index) + src1_offset;
        Time src2_dep_time = RShadowGetItem(src2_reg, index) + src2_offset;
        Time src3_dep_time = RShadowGetItem(src3_reg, index) + src3_offset;
        Time src4_dep_time = RShadowGetItem(src4_reg, index) + src4_offset;

        Time dest_time = MAX(MAX4(src1_dep_time,src2_dep_time,src3_dep_time,src4_dep_time),control_dep_time);

		RShadowSetItem(dest_time, dest_reg, index);
        RegionUpdateCp(region, dest_time);

        MSG(3, "kTime4 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src1_reg %u | src1_offset %u",src1_reg,src1_offset);
		MSG(3, " | src2_reg %u | src2_offset %u",src2_reg,src3_offset);
		MSG(3, " | src3_reg %u | src3_offset %u",src3_reg,src3_offset);
		MSG(3, " | src4_reg %u | src4_offset %u",src4_reg,src4_offset);
		MSG(3, "dest_reg %u\n", dest_reg);
        MSG(3, " src1_dep_time %u",src1_dep_time);
        MSG(3, " | src2_dep_time %u",src2_dep_time);
        MSG(3, " | src3_dep_time %u",src3_dep_time);
        MSG(3, " | src4_dep_time %u",src4_dep_time);
        MSG(3, " | dest_time %u\n",dest_time);
    }
}

void _KTimestamp5(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset) {
    MSG(3, "KTimestamp5 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u,"
	"ts[%u] + %u, ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset,src4_reg,src4_offset,src5_reg,src5_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time control_dep_time = CDepGet(index);
		assert(control_dep_time <= profiler->getCurrentTime() - region->start);

        Time src1_dep_time = RShadowGetItem(src1_reg, index) + src1_offset;
        Time src2_dep_time = RShadowGetItem(src2_reg, index) + src2_offset;
        Time src3_dep_time = RShadowGetItem(src3_reg, index) + src3_offset;
        Time src4_dep_time = RShadowGetItem(src4_reg, index) + src4_offset;
        Time src5_dep_time = RShadowGetItem(src5_reg, index) + src5_offset;

        Time dest_time =
		MAX3(MAX4(src1_dep_time,src2_dep_time,src3_dep_time,src4_dep_time),src5_dep_time,control_dep_time);

		RShadowSetItem(dest_time, dest_reg, index);
        RegionUpdateCp(region, dest_time);

        MSG(3, "kTime5 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src1_reg %u | src1_offset %u",src1_reg,src1_offset);
		MSG(3, " | src2_reg %u | src2_offset %u",src2_reg,src3_offset);
		MSG(3, " | src3_reg %u | src3_offset %u",src3_reg,src3_offset);
		MSG(3, " | src4_reg %u | src4_offset %u",src4_reg,src4_offset);
		MSG(3, " | src5_reg %u | src5_offset %u",src5_reg,src5_offset);
		MSG(3, "dest_reg %u\n", dest_reg);
        MSG(3, " src1_dep_time %u",src1_dep_time);
        MSG(3, " | src2_dep_time %u",src2_dep_time);
        MSG(3, " | src3_dep_time %u",src3_dep_time);
        MSG(3, " | src4_dep_time %u",src4_dep_time);
        MSG(3, " | src5_dep_time %u",src5_dep_time);
        MSG(3, " | dest_time %u\n",dest_time);
    }
}

void _KTimestamp6(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32
src6_reg, UInt32 src6_offset) {
    MSG(3, "KTimestamp6 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u,"
	"ts[%u] + %u, ts[%u] + %u, ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset,src4_reg,src4_offset,src5_reg,src5_offset,src6_reg,src6_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time control_dep_time = CDepGet(index);
		assert(control_dep_time <= profiler->getCurrentTime() - region->start);

        Time src1_dep_time = RShadowGetItem(src1_reg, index) + src1_offset;
        Time src2_dep_time = RShadowGetItem(src2_reg, index) + src2_offset;
        Time src3_dep_time = RShadowGetItem(src3_reg, index) + src3_offset;
        Time src4_dep_time = RShadowGetItem(src4_reg, index) + src4_offset;
        Time src5_dep_time = RShadowGetItem(src5_reg, index) + src5_offset;
        Time src6_dep_time = RShadowGetItem(src6_reg, index) + src6_offset;

        Time dest_time =
		MAX4(MAX4(src1_dep_time,src2_dep_time,src3_dep_time,src4_dep_time),src5_dep_time,src6_dep_time,control_dep_time);

		RShadowSetItem(dest_time, dest_reg, index);
        RegionUpdateCp(region, dest_time);

        MSG(3, "kTime6 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src1_reg %u | src1_offset %u",src1_reg,src1_offset);
		MSG(3, " | src2_reg %u | src2_offset %u",src2_reg,src3_offset);
		MSG(3, " | src3_reg %u | src3_offset %u",src3_reg,src3_offset);
		MSG(3, " | src4_reg %u | src4_offset %u",src4_reg,src4_offset);
		MSG(3, " | src5_reg %u | src5_offset %u",src5_reg,src5_offset);
		MSG(3, " | src6_reg %u | src6_offset %u",src6_reg,src6_offset);
		MSG(3, "dest_reg %u\n", dest_reg);
        MSG(3, " src1_dep_time %u",src1_dep_time);
        MSG(3, " | src2_dep_time %u",src2_dep_time);
        MSG(3, " | src3_dep_time %u",src3_dep_time);
        MSG(3, " | src4_dep_time %u",src4_dep_time);
        MSG(3, " | src5_dep_time %u",src5_dep_time);
        MSG(3, " | src6_dep_time %u",src6_dep_time);
        MSG(3, " | dest_time %u\n",dest_time);
    }
}

void _KTimestamp7(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32
src6_reg, UInt32 src6_offset, UInt32 src7_reg, UInt32 src7_offset) {
    MSG(3, "KTimestamp7 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u,"
	"ts[%u] + %u, ts[%u] + %u, ts[%u] + %u, ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset, src4_reg, src4_offset, src5_reg, src5_offset,
	  src6_reg, src6_offset, src7_reg, src7_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time control_dep_time = CDepGet(index);
		assert(control_dep_time <= profiler->getCurrentTime() - region->start);

        Time src1_dep_time = RShadowGetItem(src1_reg, index) + src1_offset;
        Time src2_dep_time = RShadowGetItem(src2_reg, index) + src2_offset;
        Time src3_dep_time = RShadowGetItem(src3_reg, index) + src3_offset;
        Time src4_dep_time = RShadowGetItem(src4_reg, index) + src4_offset;
        Time src5_dep_time = RShadowGetItem(src5_reg, index) + src5_offset;
        Time src6_dep_time = RShadowGetItem(src6_reg, index) + src6_offset;
        Time src7_dep_time = RShadowGetItem(src7_reg, index) + src7_offset;

		Time max_tmp1 = MAX4(src1_dep_time,src2_dep_time,src3_dep_time,src4_dep_time);
		Time max_tmp2 = MAX4(src5_dep_time,src6_dep_time,src7_dep_time,control_dep_time);
        Time dest_time = MAX(max_tmp1,max_tmp2);

		RShadowSetItem(dest_time, dest_reg, index);
        RegionUpdateCp(region, dest_time);

        MSG(3, "kTime7 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src1_reg %u | src1_offset %u",src1_reg,src1_offset);
		MSG(3, " | src2_reg %u | src2_offset %u",src2_reg,src3_offset);
		MSG(3, " | src3_reg %u | src3_offset %u",src3_reg,src3_offset);
		MSG(3, " | src4_reg %u | src4_offset %u",src4_reg,src4_offset);
		MSG(3, " | src5_reg %u | src5_offset %u",src5_reg,src5_offset);
		MSG(3, " | src6_reg %u | src6_offset %u",src6_reg,src6_offset);
		MSG(3, " | src7_reg %u | src7_offset %u",src7_reg,src7_offset);
		MSG(3, "dest_reg %u\n", dest_reg);
        MSG(3, " src1_dep_time %u",src1_dep_time);
        MSG(3, " | src2_dep_time %u",src2_dep_time);
        MSG(3, " | src3_dep_time %u",src3_dep_time);
        MSG(3, " | src4_dep_time %u",src4_dep_time);
        MSG(3, " | src5_dep_time %u",src5_dep_time);
        MSG(3, " | src6_dep_time %u",src6_dep_time);
        MSG(3, " | src7_dep_time %u",src7_dep_time);
        MSG(3, " | dest_time %u\n",dest_time);
    }
}

static inline void printTArray(Time* times, Index depth) {
	Index index;
	for (index = 0; index < depth; ++index) {
		MSG(0,"%u:%llu ",index,times[index]);
	}
}

static inline void printLoadDebugInfo(Addr addr, UInt dest, Time* times, Index depth) {
    MSG(0, "LOAD: ts[%u] = ts[0x%x] -- { ", dest, addr);
	printTArray(times,depth);
	MSG(0," }\n");
}

static inline void printStoreDebugInfo(UInt src, Addr addr, Time* times, Index depth) {
    MSG(0, "STORE: ts[0x%x] = ts[%u] -- { ", addr, src);
	printTArray(times,depth);
	MSG(0," }\n");
}

static inline void printStoreConstDebugInfo(Addr addr, Time* times, Index depth) {
    MSG(0, "STORE: ts[0x%x] = const -- { ", addr);
	printTArray(times,depth);
	MSG(0," }\n");
}

void _KLoad(Addr src_addr, Reg dest_reg, UInt32 mem_access_size, UInt32 num_srcs, ...) {
    MSG(1, "KLoad ts[%u] = max(ts[0x%x],...,ts_src%u[...]) + %u (access size: %u)\n", dest_reg,src_addr,num_srcs,LOAD_COST,mem_access_size);
	idbgAction(KREM_LOAD,"## _KLoad(src_addr=0x%x,dest_reg=%u,mem_access_size=%u,num_srcs=%u,...)\n",src_addr,dest_reg,mem_access_size,num_srcs);

    if (!isKremlinOn()) return;

	assert(mem_access_size <= 8);

	Index region_depth = profiler->getCurrNumInstrumentedLevels();
	Level min_level = profiler->getLevelForIndex(0); // XXX: this doesn't look right...
	Time* src_addr_times = mem_shadow->get(src_addr, region_depth, ProgramRegion::getVersionAtLevel(min_level), mem_access_size);

#ifdef KREMLIN_DEBUG
	printLoadDebugInfo(src_addr,dest_reg,src_addr_times,region_depth);
#endif

	// create an array holding the registers that are srcs
	Reg* src_regs = new Reg[num_srcs];

	va_list args;
	va_start(args,num_srcs);

	int src_idx;
	for(src_idx = 0; src_idx < num_srcs; ++src_idx) {
		Reg src_reg = va_arg(args,UInt32);
		src_regs[src_idx] = src_reg;
		// TODO: debug print out of src reg
	}

	Index index;
    for (index = 0; index < region_depth; index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);

		Time control_dep_time = CDepGet(index);

		// Find the maximum time of all dependencies. Dependencies include the
		// time loaded from memory as well as the times for all the "source"
		// registers that are inputs to this function. These "sources" are the
		// dependencies needed to calculate the address of the load.
		Time src_addr_time = src_addr_times[index];
		Time max_dep_time = src_addr_time;

		int src_idx;
		for(src_idx = 0; src_idx < num_srcs; ++src_idx) {
			Time src_time = RShadowGetItem(src_regs[src_idx], index);
			max_dep_time = MAX(max_dep_time,src_time);
		}

		// Take into account the cost of the load
		Time dest_time = max_dep_time + LOAD_COST;

		// TODO: more verbose printout of src dependency times
        MSG(3, "KLoad level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_addr 0x%x dest_reg %u\n", src_addr, dest_reg);
        MSG(3, " control_dep_time %u dest_time %u\n", control_dep_time, dest_time);

		// XXX: Why check timestamp here? Looks like this only occurs in KLoad
		// insts. If this is necessary, we might need to add checkTimestamp to
		// each src time (above). At the very least, we can move this check to
		// right after we calculate src_addr_time.
		checkTimestamp(index, region, src_addr_time);
        RShadowSetItem(dest_time, dest_reg, index);
        RegionUpdateCp(region, dest_time);
	}

	delete src_regs; 
}

void _KLoad0(Addr src_addr, Reg dest_reg, UInt32 mem_access_size) {
    MSG(1, "load size %d ts[%u] = ts[0x%x] + %u\n", mem_access_size, dest_reg, src_addr, LOAD_COST);
	idbgAction(KREM_LOAD, "## KLoad0(Addr=0x%x,dest_reg=%u,mem_access_size=%u)\n",
		src_addr, dest_reg, mem_access_size);

    if (!isKremlinOn()) return;

	assert(mem_access_size <= 8);

	Index region_depth = profiler->getCurrNumInstrumentedLevels();
	Level min_level = profiler->getLevelForIndex(0); // XXX: see note in KLoad
	Time* src_addr_times = mem_shadow->get(src_addr, region_depth, ProgramRegion::getVersionAtLevel(min_level), mem_access_size);

#ifdef KREMLIN_DEBUG
	printLoadDebugInfo(src_addr,dest_reg,src_addr_times,region_depth);
#endif

	Index index;
    for (index = 0; index < region_depth; index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);

		Time control_dep_time = CDepGet(index);
		Time src_addr_time = src_addr_times[index];
        Time dest_time = MAX(control_dep_time,src_addr_time) + LOAD_COST;

        MSG(3, "KLoad level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_addr 0x%x dest_reg %u\n", src_addr, dest_reg);
        MSG(3, " control_dep_time %u src_addr_time %u dest_time %u\n", control_dep_time, src_addr_time, dest_time);
#if 0
		// why are 0 to 3 hardwired in here???
		if (src_addr_time > profiler->getCurrentTime()) {
			fprintf(stderr, "@index %d, %llu, %llu, %llu, %llu\n", 
				index, src_addr_times[0], src_addr_times[1], src_addr_times[2], src_addr_times[3]);
			assert(0);
		}
#endif
		checkTimestamp(index, region, src_addr_time); // XXX: see note in KLoad0
        RShadowSetItem(dest_time, dest_reg, index);
        RegionUpdateCp(region, dest_time);
    }

    MSG(3, "load ts[%u] completed\n\n",dest_reg);
}

void _KLoad1(Addr src_addr, Reg dest_reg, Reg src_reg, UInt32 mem_access_size) {
    MSG(1, "load1 ts[%u] = max(ts[0x%x],ts[%u]) + %u\n", dest_reg, src_addr, src_reg, LOAD_COST);
	idbgAction(KREM_LOAD,"## KLoad1(Addr=0x%x,src_reg=%u,dest_reg=%u,mem_access_size=%u)\n",src_addr,src_reg,dest_reg,mem_access_size);

    if (!isKremlinOn()) return;

	assert(mem_access_size <= 8);

	Index region_depth = profiler->getCurrNumInstrumentedLevels();
    Level min_level = profiler->getMinLevel(); // XXX: KLoad/KLoad0 use profiler->getLevelForIndex(0)
	Time* src_addr_times = mem_shadow->get(src_addr, region_depth, ProgramRegion::getVersionAtLevel(min_level), mem_access_size);

#ifdef KREMLIN_DEBUG
	printLoadDebugInfo(src_addr,dest_reg,src_addr_times,region_depth);
#endif

	Index index;
    for (index = 0; index < region_depth; index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		Time control_dep_time = CDepGet(index);
		Time src_addr_time = src_addr_times[index];
		Time dep_time = RShadowGetItem(src_reg, index);

        Time max_dep_time = MAX(src_addr_time,control_dep_time);
        max_dep_time = MAX(max_dep_time,dep_time);
		Time dest_time = max_dep_time + LOAD_COST;

        MSG(3, "KLoad1 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_addr 0x%x src_reg %u dest_reg %u\n", src_addr, src_reg, dest_reg);
        MSG(3, " control_dep_time %u src_addr_time %u dep_time %u max_dep_time %u\n", control_dep_time, src_addr_time, dep_time, max_dep_time);
		checkTimestamp(index, region, src_addr_time); // XXX: see note in KLoad0
        RShadowSetItem(dest_time, dest_reg, index);
        RegionUpdateCp(region, dest_time);
    }
}

// XXX: KLoad{2,3,4} will soon be deprecated
void _KLoad2(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, UInt32 mem_access_size) {
	_KLoad0(src_addr,dest_reg,mem_access_size);
	//_KLoad(src_addr,dest_reg,mem_access_size,2,src1_reg,src2_reg);
}

void _KLoad3(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, Reg src3_reg, UInt32 mem_access_size){
	_KLoad0(src_addr,dest_reg,mem_access_size);
	//_KLoad(src_addr,dest_reg,mem_access_size,3,src1_reg,src2_reg,src3_reg);
}

void _KLoad4(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, Reg src3_reg, Reg src4_reg, UInt32 mem_access_size) { 
	_KLoad0(src_addr,dest_reg,mem_access_size);
	//_KLoad(src_addr,dest_reg,mem_access_size,4,src1_reg,src2_reg,src3_reg,src4_reg);
}


void _KStore(Reg src_reg, Addr dest_addr, UInt32 mem_access_size) {
    MSG(1, "store size %d ts[0x%x] = ts[%u] + %u\n", mem_access_size, dest_addr, src_reg, STORE_COST);
	idbgAction(KREM_STORE,"## KStore(src_reg=%u,dest_addr=0x%x,mem_access_size=%u)\n",src_reg,dest_addr,mem_access_size);

    if (!isKremlinOn()) return;

	assert(mem_access_size <= 8);

	Time* dest_addr_times = ProgramRegion::getTimeArray();

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);

		Time control_dep_time = CDepGet(index);
		Time src_time = RShadowGetItem(src_reg, index);
        Time dest_time = MAX(control_dep_time,src_time) + STORE_COST;
		dest_addr_times[index] = dest_time;
        RegionUpdateCp(region, dest_time);

// TODO: EXTRA_STATS is a MESS. Clean is up!
#ifdef EXTRA_STATS
        region->storeCnt++;
#endif

		// TODO: add more verbose debug printout with MSG
    }

#ifdef KREMLIN_DEBUG
	printStoreDebugInfo(src_reg,dest_addr,dest_addr_times,profiler->getCurrNumInstrumentedLevels());
#endif

	Level min_level = profiler->getLevelForIndex(0); // XXX: see notes in KLoads
	mem_shadow->set(dest_addr, profiler->getCurrNumInstrumentedLevels(), ProgramRegion::getVersionAtLevel(min_level), dest_addr_times, mem_access_size);
    MSG(1, "store ts[0x%x] completed\n", dest_addr);
}


void _KStoreConst(Addr dest_addr, UInt32 mem_access_size) {
    MSG(1, "KStoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
	idbgAction(KREM_STORE,"## _KStoreConst(dest_addr=0x%x,mem_access_size=%u)\n",dest_addr,mem_access_size);

    if (!isKremlinOn()) return;

	assert(mem_access_size <= 8);

	Time* dest_addr_times = ProgramRegion::getTimeArray();

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);

		// XXX: Why was the following line there but not in KStore or anywhere
		// else????
		//Index index = profiler->getCurrentLevelIndex();

		Time control_dep_time = CDepGet(index);
        Time dest_time = control_dep_time + STORE_COST;
		dest_addr_times[index] = dest_time;
		RegionUpdateCp(region, dest_time);
    }

#ifdef KREMLIN_DEBUG
	printStoreConstDebugInfo(dest_addr,dest_addr_times,profiler->getCurrNumInstrumentedLevels());
#endif

	Level min_level = profiler->getLevelForIndex(0);
	mem_shadow->set(dest_addr, profiler->getCurrNumInstrumentedLevels(), ProgramRegion::getVersionAtLevel(min_level), dest_addr_times, mem_access_size);
}



/******************************************************************
 * KPhi Functions
 *
 *  For efficiency, we use several versions with different 
 *  number of incoming dependences.
 ******************************************************************/

void _KPhi(Reg dest_reg, Reg src_reg, UInt32 num_ctrls, ...) {
    MSG(1, "KPhi ts[%u] = max(ts[%u],ts[ctrl0]...ts[ctrl%u])\n", dest_reg, src_reg,num_ctrls);
	idbgAction(KREM_PHI,"## KPhi (dest_reg=%u,src_reg=%u,num_ctrls=%u)\n",dest_reg,src_reg,num_ctrls);

    if (!isKremlinOn()) return;

	// create an array holding the registers that are srcs
	Reg* ctrl_regs = new Reg[num_ctrls]; 

	va_list args;
	va_start(args,num_ctrls);

	int ctrl_idx;
	for(ctrl_idx = 0; ctrl_idx < num_ctrls; ++ctrl_idx) {
		Reg ctrl_reg = va_arg(args,UInt32);
		ctrl_regs[ctrl_idx] = ctrl_reg;
		// TODO: debug print out of src reg
	}

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);

		Time src_time = RShadowGetItem(src_reg, index);
		Time dest_time = src_time;

		int ctrl_idx;
		for(ctrl_idx = 0; ctrl_idx < num_ctrls; ++ctrl_idx) {
			Time ctrl_time = RShadowGetItem(ctrl_regs[ctrl_idx], index);
			dest_time = MAX(dest_time,ctrl_time);
		}

		RShadowSetItem(dest_time, dest_reg, index);

        MSG(3, "KPhi level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_reg %u dest_reg %u\n", src_reg, dest_reg);
        MSG(3, " src_time %u dest_time %u\n", src_time, dest_time);
    }

	delete ctrl_regs; 
}

void _KPhi1To1(Reg dest_reg, Reg src_reg, Reg ctrl_reg) {
    MSG(1, "KPhi1To1 ts[%u] = max(ts[%u], ts[%u])\n", dest_reg, src_reg, ctrl_reg);
	idbgAction(KREM_PHI,"## KPhi1To1 (dest_reg=%u,src_reg=%u,ctrl_reg=%u)\n",dest_reg,src_reg,ctrl_reg);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);

		Time src_time = RShadowGetItem(src_reg, index);
		Time ctrl_time = RShadowGetItem(ctrl_reg, index);
        Time dest_time = MAX(src_time,ctrl_time);
		RShadowSetItem(dest_time, dest_reg, index);

        MSG(3, "KPhi1To1 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_reg %u ctrl_reg %u dest_reg %u\n", src_reg, ctrl_reg, dest_reg);
        MSG(3, " src_time %u ctrl_time %u dest_time %u\n", src_time, ctrl_time, dest_time);
    }
}

void _KPhi2To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg) {
    MSG(1, "KPhi2To1 ts[%u] = max(ts[%u], ts[%u], ts[%u])\n", dest_reg, src_reg, ctrl1_reg, ctrl2_reg);
	idbgAction(KREM_PHI,"## KPhi2To1 (dest_reg=%u,src_reg=%u,ctrl1_reg=%u,ctrl2_reg=%u)\n",dest_reg,src_reg,ctrl1_reg,ctrl2_reg);
    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);

		Time src_time = RShadowGetItem(src_reg, index);
		Time ctrl1_time = RShadowGetItem(ctrl1_reg, index);
		Time ctrl2_time = RShadowGetItem(ctrl2_reg, index);
		Time dest_time = MAX3(src_time,ctrl1_time,ctrl2_time);

		RShadowSetItem(dest_time, dest_reg, index);

        MSG(3, "KPhi2To1 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_reg %u ctrl1_reg %u ctrl2_reg %u dest_reg %u\n", src_reg, ctrl1_reg, ctrl2_reg, dest_reg);
        MSG(3, " src_time %u ctrl1_time %u ctrl2_time %u dest_time %u\n", src_time, ctrl1_time, ctrl2_time, dest_time);
    }
}

void _KPhi3To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg) {
    MSG(1, "KPhi3To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u])\n", dest_reg, src_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg);
	idbgAction(KREM_PHI,"## KPhi3To1 (dest_reg=%u,src_reg=%u,ctrl1_reg=%u,ctrl2_reg=%u,ctrl3_reg=%u)\n",dest_reg,src_reg,ctrl1_reg,ctrl2_reg,ctrl3_reg);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);

		Time src_time = RShadowGetItem(src_reg, index);
		Time ctrl1_time = RShadowGetItem(ctrl1_reg, index);
		Time ctrl2_time = RShadowGetItem(ctrl2_reg, index);
		Time ctrl3_time = RShadowGetItem(ctrl3_reg, index);
		Time dest_time = MAX4(src_time,ctrl1_time,ctrl2_time,ctrl3_time);
		RShadowSetItem(dest_time, dest_reg, index);

        MSG(3, "KPhi3To1 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_reg %u ctrl1_reg %u ctrl2_reg %u ctrl3_reg %u dest_reg %u\n", src_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg, dest_reg);
        MSG(3, " src_time %u ctrl1_time %u ctrl2_time %u ctrl3_time %u dest_time %u\n", src_time, ctrl1_time, ctrl2_time, ctrl3_time, dest_time);
    }
}

void _KPhi4To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg) {
    MSG(1, "KPhi4To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest_reg, src_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg, ctrl4_reg);
	idbgAction(KREM_PHI,"## KPhi4To1 (dest_reg=%u,src_reg=%u,ctrl1_reg=%u,ctrl2_reg=%u,ctrl3_reg=%u,ctrl4_reg=%u)\n", dest_reg,src_reg,ctrl1_reg,ctrl2_reg,ctrl3_reg,ctrl4_reg);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);

		Time src_time = RShadowGetItem(src_reg, index);
		Time ctrl1_time = RShadowGetItem(ctrl1_reg, index);
		Time ctrl2_time = RShadowGetItem(ctrl2_reg, index);
		Time ctrl3_time = RShadowGetItem(ctrl3_reg, index);
		Time ctrl4_time = RShadowGetItem(ctrl4_reg, index);
		// TODO: MAX5???
		Time dest_time = MAX(src_time,MAX4(ctrl1_time,ctrl2_time,ctrl3_time,ctrl4_time));
		RShadowSetItem(dest_time, dest_reg, index);

        MSG(2, "KPhi4To1 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(2, " src_reg %u ctrl1_reg %u ctrl2_reg %u ctrl3_reg %u ctrl4_reg %u dest_reg %u\n", src_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg, ctrl4_reg, dest_reg);
        MSG(2, " src_time %u ctrl1_time %u ctrl2_time %u ctrl3_time %u ctrl4_time %u dest_time %u\n", 
			src_time, ctrl1_time, ctrl2_time, ctrl3_time, ctrl4_time, dest_time);
    }
}

void _KPhiCond4To1(Reg dest_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg) {
    MSG(1, "KPhi4To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest_reg, dest_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg, ctrl4_reg);
	idbgAction(KREM_CD_TO_PHI,"## KPhi4To1 (dest_reg=%u,ctrl1_reg=%u,ctrl2_reg=%u,ctrl3_reg=%u,ctrl4_reg=%u)\n",
		dest_reg,ctrl1_reg,ctrl2_reg,ctrl3_reg,ctrl4_reg);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);

        Time old_dest_time = RShadowGetItem(dest_reg, index);
		Time ctrl1_time = RShadowGetItem(ctrl1_reg, index);
		Time ctrl2_time = RShadowGetItem(ctrl2_reg, index);
		Time ctrl3_time = RShadowGetItem(ctrl3_reg, index);
		Time ctrl4_time = RShadowGetItem(ctrl4_reg, index);
		// TODO: MAX5???
		Time new_dest_time = MAX(old_dest_time,MAX4(ctrl1_time,ctrl2_time,ctrl3_time,ctrl4_time));
		RShadowSetItem(new_dest_time, dest_reg, index);

        MSG(3, "KPhi4To1 level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " ctrl1_reg %u ctrl2_reg %u ctrl3_reg %u ctrl4_reg %u dest_reg %u\n", ctrl1_reg, ctrl2_reg, ctrl3_reg, ctrl4_reg, dest_reg);
        MSG(3, " old_dest_time %u ctrl1_time %u ctrl2_time %u ctrl3_time %u ctrl4_time %u new_dest_time %u\n", old_dest_time, ctrl1_time, ctrl2_time, ctrl3_time, ctrl4_time, new_dest_time);
    }
}

void _KPhiAddCond(Reg dest_reg, Reg src_reg) {
    MSG(1, "KPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest_reg, src_reg, dest_reg);
	idbgAction(KREM_CD_TO_PHI,"## KPhiAddCond (dest_reg=%u,src_reg=%u)\n",dest_reg,src_reg);

    if (!isKremlinOn()) return;

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Level i = profiler->getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);

		Time src_time = RShadowGetItem(src_reg, index);
		Time old_dest_time = RShadowGetItem(dest_reg, index);
        Time new_dest_time = MAX(src_time,old_dest_time);
		RShadowSetItem(new_dest_time, dest_reg, index);

        RegionUpdateCp(region, new_dest_time);

        MSG(3, "KPhiAddCond level %u version %u \n", i, *ProgramRegion::getVersionAtLevel(i));
        MSG(3, " src_reg %u dest_reg %u\n", src_reg, dest_reg);
        MSG(3, " src_time %u old_dest_time %u new_dest_time %u\n", src_time, old_dest_time, new_dest_time);
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
			mem_shadow = new MShadowBase();
			break;
		case 1:
			mem_shadow = new MShadowSTV();
			break;
		case 2:
			mem_shadow = new MShadowSkadu();
			break;
		default:
			mem_shadow = new MShadowDummy();
	}
	mem_shadow->init();
}

void MShadowDeinit() {
	mem_shadow->deinit();
	delete mem_shadow;
	mem_shadow = NULL;
}

static UInt hasInitialized = 0;

#define REGION_INIT_SIZE	64

static bool kremlinInit() {
	DebugInit();
    if(hasInitialized++) {
        MSG(0, "kremlinInit skipped\n");
        return false;
    }

	profiler = new KremlinProfiler(KConfigGetMinLevel(), KConfigGetMaxLevel());

    MSG(0, "Profile Level = (%d, %d), Index Size = %d\n", 
        profiler->getMinLevel(), profiler->getMaxLevel(), profiler->getArraySize());
    MSG(0, "kremlinInit running....");
	if(KConfigGetDebug()) { 
		fprintf(stderr,"[kremlin] debugging enabled at level %d\n", KConfigGetDebugLevel()); 
	}

	profiler->initFunctionArgQueue();
	CDepInit();
	CRegionInit();
	RShadowInit(profiler->getArraySize());

	MShadowInit(/*KConfigGetSkaduCacheSize()*/); // XXX: what was this arg for?
	ProgramRegion::initProgramRegions(REGION_INIT_SIZE);
   	_KTurnOn();
    return true;
}

/*
   if a program exits out of main(),
   kremlinCleanup() enforces 
   KExitRegion() calls for active regions
 */
void kremlinCleanup() {
    Level level = profiler->getCurrentLevel();
	int i;
	for (i=level; i>=0; i--) {
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
		_KExitRegion(region->regionId, region->regionType);
	}
}

static bool kremlinDeinit() {
	kremlinCleanup();
    if(--hasInitialized) {
        MSG(0, "kremlinDeinit skipped\n");
        return false;
    }

	fprintf(stderr,"[kremlin] max active level = %d\n", 
		profiler->getMaxActiveLevel());	

	_KTurnOff();
	CRegionDeinit(KConfigGetOutFileName());
	RShadowDeinit();
	MShadowDeinit();
	profiler->deinitFunctionArgQueue();
	CDepDeinit();
	ProgramRegion::deinitProgramRegions();
    //MemMapAllocatorDelete(&memPool);
	
	delete profiler;

	DebugDeinit();
    return true;
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


#define MAX_ENTRY 10

// use estimated cost for a callee function we cannot instrument
// TODO: implement new shadow mem interface
void _KCallLib(UInt cost, UInt dest, UInt num_in, ...) {
	idbgAction(KREM_CD_TO_PHI,"## KCallLib(cost=%u,dest=%u,num_in=%u,...)\n",cost,dest,num_in);

#if 0
    if (!isKremlinOn())
        return;

    MSG(1, "KCallLib to ts[%u] with cost %u\n", dest, cost);
    _KWork(cost);

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

    int minLevel = profiler->getMinLevel();
    int maxLevel = getEndLevel();

    TEntryRealloc(entryDest, maxLevel);
    for (i = minLevel; i <= maxLevel; i++) {
        UInt version = *ProgramRegion::getVersionAtLevel(i);
        UInt64 max = 0;
        
		/*
        int j;
        for (j = 0; j < num_in; j++) {
            UInt64 ts = profiler->getCurrentTime(entrySrc[j], i, version);
            if (ts > max)
                max = ts;
        } */  
        
		UInt64 value = max + cost;

		updateCP(value, i);
    }
    return;
#endif
    
}

/***********************************************
 * Dynamic Memory Allocation / Deallocation
 * DJ: will be optimized later..
 ************************************************/


// FIXME: support 64 bit address
void _KMalloc(Addr addr, size_t size, UInt dest) {
	// TODO: idbgAction
#if 0
    if (!isKremlinOn()) return;
    
    MSG(1, "KMalloc addr=0x%x size=%llu\n", addr, (UInt64)size);

    // Don't do anything if malloc returned NULL
    if(!addr) { return; }

    createMEntry(addr,size);
#endif
}

// TODO: implement for new shadow mem interface
void _KFree(Addr addr) {
	// TODO: idbgAction
#if 0
    if (!isKremlinOn()) return;

    MSG(1, "KFree addr=0x%x\n", addr);

    // Calls to free with NULL just return.
    if(addr == NULL) return;

    size_t mem_size = getMEntry(addr);

    Addr a;
    for(a = addr; a < addr + mem_size; a++) {
        GTableDeleteTEntry(gTable, a);
	}

    freeMEntry(addr);
	_KWork(FREE_COST);
	// make sure CP is at least the time needed to complete the free
    int minLevel = profiler->getMinLevel();
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
	// TODO: idbgAction
#if 0
    if (!isKremlinOn())
        return;

    MSG(1, "KRealloc old_addr=0x%x new_addr=0x%x size=%llu\n", old_addr, new_addr, (UInt64)size);
    _KFree(old_addr);
    _KMalloc(new_addr,size,dest);
#endif
}

/***********************************************
 * Kremlin Interactive Debugger Functions 
 ************************************************/
void printActiveRegionStack() {
	fprintf(stdout,"Current Regions:\n");

	int i;
	Level level = profiler->getCurrentLevel();

	for(i = 0; i <= level; ++i) {
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);
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

    	UInt64 work = profiler->getCurrentTime() - region->start;
		fprintf(stdout,"SID=%llu, WORK'=%llu, CP=%llu\n",region->regionId,work,region->cp);
	}
}

void printControlDepTimes() {
	fprintf(stdout,"Control Dependency Times:\n");
	Index index;

    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Time cdt = CDepGet(index);
		fprintf(stdout,"\t#%u: %llu\n",index,cdt);
	}
}

void printRegisterTimes(Reg reg) {
	fprintf(stdout,"Timestamps for reg[%u]:\n",reg);

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
        Time ts = RShadowGetItem(reg, index);
		fprintf(stdout,"\t#%u: %llu\n",index,ts);
	}
}

void printMemoryTimes(Addr addr, Index size) {
	fprintf(stdout,"Timestamps for Mem[%x]:\n",addr);
	Index index;
	Index depth = profiler->getCurrNumInstrumentedLevels();
	Level minLevel = profiler->getLevelForIndex(0);
	Time* tArray = mem_shadow->get(addr, depth, ProgramRegion::getVersionAtLevel(minLevel), size);

    for (index = 0; index < depth; index++) {
		Time ts = tArray[index];
		fprintf(stdout,"\t#%u: %llu\n",index,ts);
	}
}

#if 0
/***********************************************
 * DJ: not sure what these are for 
 ************************************************/

void* _KInsertValue(UInt src, UInt dest) {
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

void* _KInsertValueConst(UInt dest) {
	assert(0);
    //printf("Warning: _KInsertValueConst not correctly implemented\n");

#ifdef KREMLIN_DEBUG
	if(__kremlin_idbg) {
		if(__kremlin_idbg_run_state == Waiting) {
    		fprintf(stdout, "## _KInsertValueConst(dest=%u)\n\t",dest);
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

bool isCpp = false;

void cppEntry() {
    isCpp = true;
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

static std::vector<InvokeRecord*> invokeRecords; // A vector used to record invoked calls.

void _KPrepInvoke(UInt64 id) {
    if(!isKremlinOn())
        return;

    MSG(1, "prepareInvoke(%llu) - saved at %lld\n", id, (UInt64)profiler->getCurrentLevel());
   
    InvokeRecord* currentRecord = InvokeRecordsPush(invokeRecords); // FIXME
    currentRecord->id = id;
    currentRecord->stackHeight = profiler->getCurrentLevel();
}

void _KInvokeOkay(UInt64 id) {
    if(!isKremlinOn())
        return;

    if(!invokeRecords.empty() && invokeRecords.back()->id == id) {
        MSG(1, "invokeOkay(%u)\n", id);
		invokeRecords.pop_back();
    } else
        MSG(1, "invokeOkay(%u) ignored\n", id);
}

void _KInvokeThrew(UInt64 id)
{
    if(!isKremlinOn())
        return;

    fprintf(stderr, "invokeRecordOnTop: %u\n", invokeRecords.back()->id);

    if(!invokeRecords.empty() && invokeRecords.back()->id == id) {
        InvokeRecord* currentRecord = invokeRecords.back();
        MSG(1, "invokeThrew(%u) - Popping to %d\n", currentRecord->id, currentRecord->stackHeight);
        while(profiler->getCurrentLevel() > currentRecord->stackHeight)
        {
            UInt64 lastLevel = profiler->getCurrentLevel();
            ProgramRegion* region = regionInfo + getLevelOffset(profiler->getCurrentLevel()); // FIXME: regionInfo is vector now
            _KExitRegion(region->regionId, region->regionType);
            assert(profiler->getCurrentLevel() < lastLevel);
            assert(profiler->getCurrentLevel() >= 0);
        }
		invokeRecods.pop_back();
    }
    else
        MSG(1, "invokeThrew(%u) ignored\n", id);
}

#endif




#endif 

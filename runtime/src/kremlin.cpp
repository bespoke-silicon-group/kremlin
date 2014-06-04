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

#include "Table.h"
#include "RShadow.h"
#include "PoolAllocator.hpp"

#include "KremlinProfiler.hpp"
#include "ProgramRegion.hpp"
#include "FunctionRegion.hpp"

#include <vector>
#include <iostream>
#include <signal.h> // for catching CTRL-V during debug


static KremlinProfiler *profiler;
KremlinConfiguration kremlin_config;

extern "C" int __main(int argc, char** argv);

static void initProfiler() {
	profiler = new KremlinProfiler(kremlin_config.getMinProfiledLevel(), 
					kremlin_config.getMaxProfiledLevel());
	profiler->init();
}


/*!
 * @brief The starting point for Kremlin's profiling.
 *
 * This function performs the following actions:
 * 1) Search for any kremlin specific parameters and use those to modify our 
 * KremlinConfiguration, keeping a list of the "true" (i.e. non-kremlin)
 * arguments during this search.
 * 2) Initialize the KremlinProfiler.
 * 3) Call the program's original main function with only the true args.
 * 4) Deinitialize the KremlinProfiler.
 * 
 * @param argc The number of arguments (both kremlin and program specific)
 * @param argv Array of strings for each argument (again, both kremlin and
 * program specific).
 * @return 0 on success, non-0 otherwise.
 */
int main(int argc, char* argv[]) {
	__kremlin_idbg = 0;

	std::vector<char*> program_args;
	parseKremlinOptions(kremlin_config, argc, argv, program_args);

	if(__kremlin_idbg == 0) {
		(void)signal(SIGINT,dbg_int);
	}
	else {
		fprintf(stderr,"[kremlin] Interactive debugging mode enabled.\n");
	}

	kremlin_config.print();

#if 0
	for (unsigned i = 0; i < program_args.size(); ++i) {
		MSG(0,"program arg %u: %s\n", i, program_args[i]);
	}
#endif

	if (profiler == NULL) initProfiler();
	profiler->enable();

	__main(program_args.size(), &program_args[0]);

	profiler->deinit();
	delete profiler;
	profiler = NULL;
}

// XXX: hacky... badness!
Level getMaxActiveLevel() {
	return profiler->getMaxActiveLevel();
}

// BEGIN TODO: make these not global
static UInt64	loadCnt = 0llu;
static UInt64	storeCnt = 0llu;
// END TODO: make these not global


FunctionRegion* KremlinProfiler::getCurrentFunction() {
	if (callstack.empty()) {
		MSG(3, "getCurrentFunction  NULL\n");
		return NULL;
	}

	FunctionRegion* func = callstack.back();

	MSG(3, "getCurrentFunction  0x%x CID 0x%x\n", func, func->getCallSiteID());
	func->sanityCheck();
	return func;
}

FunctionRegion* KremlinProfiler::getCallingFunction() {
	if (callstack.size() == 1) {
		MSG(3, "getCallingFunction  No Caller Context\n");
		return NULL;
	}
	FunctionRegion* func = callstack[callstack.size()-2];

	MSG(3, "getCallingFunction  0x%x CID 0x%x\n", func, func->getCallSiteID());
	func->sanityCheck();
	return func;
}

void KremlinProfiler::initShadowMemory() {
	switch(kremlin_config.getShadowMemType()) {
		case ShadowMemoryBase:
			shadow_mem = new MShadowBase();
			break;
		case ShadowMemorySTV:
			shadow_mem = new MShadowSTV();
			break;
		case ShadowMemorySkadu:
			shadow_mem = new MShadowSkadu();
			break;
		default:
			shadow_mem = new MShadowDummy();
	}
	shadow_mem->init();
}

void KremlinProfiler::deinitShadowMemory() {
	shadow_mem->deinit();
	delete shadow_mem;
	shadow_mem = NULL;
}



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

ProgramRegion* KremlinProfiler::getRegionAtLevel(Level l) {
	assert(l < program_regions.size());
	ProgramRegion* ret = program_regions[l];
	ret->sanityCheck();
	return ret;
}

void KremlinProfiler::increaseNumRegions(unsigned num_new) {
	for (unsigned i = 0; i < num_new; ++i) {
		program_regions.push_back(new ProgramRegion());
	}
}

void KremlinProfiler::deinitProgramRegions() { 
	for (unsigned i = 0; i < program_regions.size(); ++i)
		delete program_regions[i];
	program_regions.clear();
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

void _KPushCDep(Reg cond) {
	profiler->handlePushCDep(cond);
}
void _KPopCDep() {
	profiler->handlePopCDep();
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
	profiler->handlePrepCall(callSiteId, calledRegionId);
}

void _KEnqArg(Reg src) {
	profiler->handleEnqueueArgument(src);
}

void _KEnqArgConst() {
	profiler->handleEnqueueConstArgument();
}

void _KDeqArg(Reg dest) {
	profiler->handleDequeueArgument(dest);
}

void _KPrepRTable(UInt maxVregNum, UInt maxNestLevel) {
	profiler->handlePrepRTable(maxVregNum, maxNestLevel);
}

void _KLinkReturn(Reg dest) {
	profiler->handleLinkReturn(dest);
}

void _KReturn(Reg src) {
	profiler->handleReturn(src);
}

void _KReturnConst() {
	profiler->handleReturnConst();
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
 * when kremlin is disabled, most instrumentation functions do nothing.
 */ 
void _KTurnOn() {
	profiler->enable();
    MSG(0, "_KTurnOn\n");
	fprintf(stderr, "[kremlin] Logging started.\n");
}

/**
 * end profiling
 */
void _KTurnOff() {
	profiler->disable();
    MSG(0, "_KTurnOff\n");
	fprintf(stderr, "[kremlin] Logging stopped.\n");
}


/*****************************************************************
 * KEnterRegion / KExitRegion
 *****************************************************************/

void _KEnterRegion(SID regionId, RegionType regionType) {
	// @TRICKY: In C++ some instrumented object constructors may be called
	// before main. We need to make sure that profiler is not NULL whenever we
	// have an API call. Luckily, we are guaranteed that KEnterRegion will be
	// the kremlin first function called in a constructor. We'll take
	// advantage of that invariant and initialize the profiler here if we find 
	// that it hasn't been initialized yet.
	// Note that initProfiler doesn't enable profiling so we won't actual
	// profile any of the code in the pre-main constructors (just like we
	// won't profile any code in post-main destructors)
	if (profiler == NULL) initProfiler();
	profiler->handleRegionEntry(regionId, regionType);
}

/**
 * Handles exiting of regions. This includes handling function context, calculating profiled
 * statistics, and logging region statistics.
 * @param regionID		ID of region that is being exited.
 * @param regionType	Type of region being exited.
 */
void _KExitRegion(SID regionId, RegionType regionType) {
	profiler->handleRegionExit(regionId, regionType);
}

void _KLandingPad(SID regionId, RegionType regionType) {
	profiler->handleLandingPad(regionId, regionType);
}

/*****************************************************************
 * KInduction, KReduction, KTimestamp, KAssignConst
 *****************************************************************/

void _KAssignConst(UInt dest_reg) {
	profiler->handleAssignConst(dest_reg);
}
void _KInduction(UInt dest_reg) {
	profiler->handleInduction(dest_reg);
}
void _KReduction(UInt op_cost, Reg dest_reg) {
	profiler->handleReduction(op_cost, dest_reg);
}

void _KTimestamp(UInt32 dest_reg, UInt32 num_srcs, ...) {
	va_list args;
	va_start(args,num_srcs);
	profiler->handleTimestamp(dest_reg, num_srcs, args);
	va_end(args);
}
void _KTimestamp0(UInt32 dest_reg) {
	profiler->handleTimestamp0(dest_reg);
}
void _KTimestamp1(UInt32 dest_reg, UInt32 src_reg, UInt32 src_offset) {
	profiler->handleTimestamp1(dest_reg, src_reg, src_offset);
}
void _KTimestamp2(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset) {
	profiler->handleTimestamp2(dest_reg, src1_reg, src1_offset, src2_reg, src2_offset);
}
void _KTimestamp3(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset) {
	profiler->handleTimestamp3(dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg, src3_offset);
}
void _KTimestamp4(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32 src4_reg, UInt32 src4_offset) {
	profiler->handleTimestamp4(dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg, src3_offset, src4_reg, src4_offset);
}
void _KTimestamp5(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32 src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset) {
	profiler->handleTimestamp5(dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg, src3_offset, src4_reg, src4_offset, src5_reg, src5_offset);
}
void _KTimestamp6(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32 src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32 src6_reg, UInt32 src6_offset) {
	profiler->handleTimestamp6(dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg, src3_offset, src4_reg, src4_offset, src5_reg, src5_offset, src6_reg, src6_offset);
}
void _KTimestamp7(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, 
					UInt32 src2_reg, UInt32 src2_offset, 
					UInt32 src3_reg, UInt32 src3_offset, 
					UInt32 src4_reg, UInt32 src4_offset, 
					UInt32 src5_reg, UInt32 src5_offset, 
					UInt32 src6_reg, UInt32 src6_offset, 
					UInt32 src7_reg, UInt32 src7_offset) {
	profiler->handleTimestamp7(dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg, src3_offset, src4_reg, src4_offset, src5_reg, src5_offset, src6_reg, src6_offset, src7_reg, src7_offset);
}


void _KLoad(Addr src_addr, Reg dest_reg, UInt32 mem_access_size, UInt32 num_srcs, ...) {
	va_list args;
	va_start(args,num_srcs);
	profiler->handleLoad(src_addr, dest_reg, mem_access_size, num_srcs, args);
	va_end(args);
}
void _KLoad0(Addr src_addr, Reg dest_reg, UInt32 mem_access_size) {
	profiler->handleLoad0(src_addr, dest_reg, mem_access_size);
}
void _KLoad1(Addr src_addr, Reg dest_reg, Reg src_reg, UInt32 mem_access_size) {
	profiler->handleLoad1(src_addr, dest_reg, src_reg, mem_access_size);
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
	profiler->handleStore(src_reg, dest_addr, mem_access_size);
}
void _KStoreConst(Addr dest_addr, UInt32 mem_access_size) {
	profiler->handleStoreConst(dest_addr, mem_access_size);
}

/******************************************************************
 * KPhi Functions
 *
 *  For efficiency, we use several versions with different 
 *  number of incoming dependences.
 ******************************************************************/

// TODO: shouldn't call KPhi if num_ctrls is 0 (instrumentation issue)
void _KPhi(Reg dest_reg, Reg src_reg, UInt32 num_ctrls, ...) {
	va_list args;
	va_start(args, num_ctrls);
	profiler->handlePhi(dest_reg, src_reg, num_ctrls, args);
	va_end(args);
}

void _KPhi1To1(Reg dest_reg, Reg src_reg, Reg ctrl_reg) {
	profiler->handlePhi1To1(dest_reg, src_reg, ctrl_reg);
}
void _KPhi2To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg) {
	profiler->handlePhi2To1(dest_reg, src_reg, ctrl1_reg, ctrl2_reg);
}
void _KPhi3To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg) {
	profiler->handlePhi3To1(dest_reg, src_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg);
}
void _KPhi4To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg) {
	profiler->handlePhi4To1(dest_reg, src_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg, ctrl4_reg);
}

void _KPhiCond4To1(Reg dest_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg) {
	profiler->handlePhiCond4To1(dest_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg, ctrl4_reg);
}

void _KPhiAddCond(Reg dest_reg, Reg src_reg) {
	profiler->handlePhiAddCond(dest_reg, src_reg);
}

/******************************
 * Kremlin Init / Deinit
 *****************************/

// TODO: these functions no longer have a use. Remove them from API.
void _KInit() {}
void _KDeinit() {}

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
    if (!profiler->isEnabled())
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
    if (!profiler->isEnabled()) return;
    
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
    if (!profiler->isEnabled()) return;

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
        UInt64 value = profiler->getControlDependenceAtIndex(i) + FREE_COST;

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
    if (!profiler->isEnabled())
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

// got rid of this because getRegionAtLevel is private to KremlinProfiler now.
// TODO: resurrect this somehow
#if 0
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
#endif
}

void printControlDepTimes() {
	fprintf(stdout,"Control Dependency Times:\n");
	Index index;

    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
		Time cdt = profiler->getControlDependenceAtIndex(index);
		fprintf(stdout,"\t#%u: %llu\n",index,cdt);
	}
}

void printRegisterTimes(Reg reg) {
	fprintf(stdout,"Timestamps for reg[%u]:\n",reg);

	Index index;
    for (index = 0; index < profiler->getCurrNumInstrumentedLevels(); index++) {
        Time ts = profiler->getRegisterTimeAtIndex(reg, index);
		fprintf(stdout,"\t#%u: %llu\n",index,ts);
	}
}

void printMemoryTimes(Addr addr, Index size) {
	fprintf(stdout,"Timestamps for Mem[%p]:\n",addr);
// again, does nothing because moved program region stuff to KremlinProfiler
// TODO: resurrect
#if 0
	Index index;
	Index depth = profiler->getCurrNumInstrumentedLevels();
	Level minLevel = profiler->getLevelForIndex(0);
	Time* tArray = profiler->getShadowMemory()->get(addr, depth, ProgramRegion::getVersionAtLevel(minLevel), size);

    for (index = 0; index < depth; index++) {
		Time ts = tArray[index];
		fprintf(stdout,"\t#%u: %llu\n",index,ts);
	}
#endif
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
    profiler->init();
}

void cppExit() {
    profiler->deinit();
}

typedef struct _InvokeRecord {
	UInt64 id;
	int stackHeight;
} InvokeRecord;

InvokeRecords*      invokeRecords;

static std::vector<InvokeRecord*> invokeRecords; // A vector used to record invoked calls.

void _KPrepInvoke(UInt64 id) {
    if(!profiler->isEnabled())
        return;

    MSG(1, "prepareInvoke(%llu) - saved at %lld\n", id, (UInt64)profiler->getCurrentLevel());
   
    InvokeRecord* currentRecord = InvokeRecordsPush(invokeRecords); // FIXME
    currentRecord->id = id;
    currentRecord->stackHeight = profiler->getCurrentLevel();
}

void _KInvokeOkay(UInt64 id) {
    if(!profiler->isEnabled())
        return;

    if(!invokeRecords.empty() && invokeRecords.back()->id == id) {
        MSG(1, "invokeOkay(%u)\n", id);
		invokeRecords.pop_back();
    } else
        MSG(1, "invokeOkay(%u) ignored\n", id);
}

void _KInvokeThrew(UInt64 id)
{
    if(!profiler->isEnabled())
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

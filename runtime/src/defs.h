//#define NDEBUG
#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "MemMapAllocator.h"


#ifndef _PYRPROF_DEF
#define _PYRPROF_DEF

// unless specified, we assume no debugging
#ifndef PYRPROF_DEBUG
#define PYRPROF_DEBUG	0
#endif

#ifndef DEBUGLEVEL
#define DEBUGLEVEL		0
#endif

// save the last visited BB number 
// for now we do not use it
// but if we need it later, define it 

//#define MANAGE_BB_INFO	

#define TRUE 1
#define FALSE 0

//#define USE_UREGION

#ifndef MAX_REGION_LEVEL
#define MAX_REGION_LEVEL	20		
#endif

#ifndef MIN_REGION_LEVEL
#define MIN_REGION_LEVEL	0
#endif

#define LOAD_COST			4
#define STORE_COST			1
#define MALLOC_COST			100
#define FREE_COST			10

#define CACHE_LINE_POWER_2	4
#define CACHE_LINE_SIZE		(1 << CACHE_LINE_POWER_2)

typedef unsigned long       UInt32;
typedef signed long         Int32;
typedef unsigned int		UInt;
typedef signed int          Int;
typedef unsigned long long  UInt64;
typedef signed long long    Int64;
typedef void*               Addr;
typedef FILE                File;

enum RegionType {RegionFunc, RegionLoop};
MemMapAllocator* memPool;

typedef struct _RegionField_t {
	UInt64 work;
	UInt64 cp;
	UInt64 callSite;
	UInt64 spWork;
	UInt64 tpWork;

#ifdef EXTRA_STATS
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 readLineCnt;
#endif
} RegionField;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* The following funcs are inserted by the critical path instrumentation pass */
void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest); 
void* logBinaryOpConst(UInt opCost, UInt src, UInt dest); 

void* logAssignment(UInt src, UInt dest);
void* logAssignmentConst(UInt dest); 

void* logInsertValue(UInt src, UInt dest); 
void* logInsertValueConst(UInt dest); 

void* logLoadInst(Addr src_addr, UInt dest); 
void* logStoreInst(UInt src, Addr dest_addr); 
void* logStoreInstConst(Addr dest_addr); 

void logMalloc(Addr addr, size_t size, UInt dest);
void logRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest);
void logFree(Addr addr);

void* logPhiNode1CD(UInt dest, UInt src, UInt cd); 
void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2); 
void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3); 
void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4); 

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4);

void logPhiNodeAddCondition(UInt dest, UInt src);

void prepareInvoke(UInt64);
void invokeThrew(UInt64);
void invokeOkay(UInt64);

void addControlDep(UInt cond);
void removeControlDep();

void prepareCall(UInt64, UInt64);
void addReturnValueLink(UInt dest);
void logFuncReturn(UInt src); 
void logFuncReturnConst(void);

void linkArgToLocal(UInt src); 
void linkArgToConst(void);
void transferAndUnlinkArg(UInt dest); 

void* logLibraryCall(UInt cost, UInt dest, UInt num_in, ...); 

void logBBVisit(UInt bb_id); 

void* logInductionVar(UInt dest); 
void* logInductionVarDependence(UInt induct_var); 

void* logReductionVar(UInt opCost, UInt dest); 

/* The following functions are inserted by region instrumentation pass */
void initProfiler();
void deinitProfiler();

void turnOnProfiler();
void turnOffProfiler();

void logRegionEntry(UInt64 region_id, UInt region_type);
void logRegionExit(UInt64 region_id, UInt region_type);

// the following two functions are part of our plans for c++ support
void cppEntry();
void cppExit();


// tempoarary use only
// setup local table. 
void setupLocalTable(UInt maxVregNum);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif

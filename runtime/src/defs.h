#ifndef _DEFS_H
#define _DEFS_H
#define NDEBUG
//#define KREMLIN_DEBUG

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>

#ifndef _KREMLIN_DEF
#define _KREMLIN_DEF

//#define INIT_LEVEL_DEPTH	64
// save the last visited BB number 
// for now we do not use it
// but if we need it later, define it 

//#define MANAGE_BB_INFO	

#define TRUE 1
#define FALSE 0

#define LOAD_COST			4
#define STORE_COST			1
#define MALLOC_COST			100
#define FREE_COST			10

#define CACHE_LINE_POWER_2	4
#define CACHE_LINE_SIZE		(1 << CACHE_LINE_POWER_2)

typedef uint16_t 	       	UInt16;
typedef uint32_t 	       	UInt32;
typedef int32_t         	Int32;
typedef uint32_t			UInt;
typedef uint8_t				UInt8;
typedef signed int          Int;
typedef uint64_t			UInt64;
typedef int64_t				Int64;
typedef UInt32 				Bool;
typedef void*               Addr;
typedef FILE                File;
typedef UInt64 				Timestamp;
typedef UInt64 				Time;
typedef UInt64 				Version;
typedef UInt32 				Level;
typedef UInt32 				Index;
typedef UInt 				Reg;
typedef UInt64				SID; 	// static region ID
typedef UInt64				CID;	// callsite ID



extern Time* (*MShadowGet)(Addr, Index, Version*, UInt32);
extern void  (*MShadowSet)(Addr, Index, Version*, Time*, UInt32) ;




typedef enum RegionType {RegionFunc, RegionLoop, RegionLoopBody} RegionType;


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

void* logLoadInst(Addr src_addr, UInt dest, UInt32 size); 
void* logLoadInst1Src(Addr src_addr, UInt src1, UInt dest, UInt32 size);
void* logLoadInst2Src(Addr src_addr, UInt src1, UInt src2, UInt dest, UInt32 size);
void* logLoadInst3Src(Addr src_addr, UInt src1, UInt src2, UInt src3, UInt dest, UInt32 size);
void* logLoadInst4Src(Addr src_addr, UInt src1, UInt src2, UInt src3, UInt src4, UInt dest, UInt32 size);
void* logStoreInst(UInt src, Addr dest_addr, UInt32 size); 
void* logStoreInstConst(Addr dest_addr, UInt32 size); 

void logMalloc(Addr addr, size_t size, UInt dest);
void logRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest);
void logFree(Addr addr);

void* logPhiNode1CD(UInt dest, UInt src, UInt cd); 
void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2); 
void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3); 
void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4); 

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4);

void* logPhiNodeAddCondition(UInt dest, UInt src);

void prepareInvoke(UInt64);
void invokeThrew(UInt64);
void invokeOkay(UInt64);

void addControlDep(Reg cond);
void removeControlDep();

void prepareCall(UInt64, UInt64);
void addReturnValueLink(Reg dest);
void logFuncReturn(Reg src); 
void logFuncReturnConst(void);

void linkArgToLocal(Reg src); 
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

/* level management */
Level getMinReportLevel();
Level getMaxReportLevel();

// the following two functions are part of our plans for c++ support
void cppEntry();
void cppExit();


// tempoarary use only
// setup local table. 
void setupLocalTable(UInt maxVregNum);


// utility functions
extern Level __kremlin_min_level;
extern Level __kremlin_max_level;

#define getMinLevel() (__kremlin_min_level)
#define getMaxLevel() (__kremlin_max_level)
#define getIndexSize() (__kremlin_max_level - __kremlin_min_level + 1)
#define getLevel(index) (index + __kremlin_min_level)



#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif

#endif

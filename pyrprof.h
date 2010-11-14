#ifndef PYRPROF_H
#define PYRPROF_H

#include "defs.h"
#include "table.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version);
void updateTimestamp(TEntry* entry, UInt32 level, UInt32 version, UInt64 timestamp);

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

void logMalloc(Addr addr, size_t size);
void logRealloc(Addr old_addr, Addr new_addr, size_t size);
void logFree(Addr addr);

void* logPhiNode1CD(UInt dest, UInt src, UInt cd); 
void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2); 
void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3); 
void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4); 

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4);

void logPhiNodeAddCondition(UInt dest, UInt src);

void prepareInvoke(UInt);
void invokeThrew(UInt);
void invokeOkay(UInt);

void addControlDep(UInt cond);
void removeControlDep();

void prepareCall(void);
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

void logRegionEntry(UInt region_id, UInt region_type);
void logRegionExit(UInt region_id, UInt region_type);

void logLoopIteration();

// the following two functions are part of our plans for c++ support
void cppEntry();
void cppExit();

// tempoarary use only
// setup local table. 
void setupLocalTable(UInt maxVregNum);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif /* PYRPROF_H */

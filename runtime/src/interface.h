#ifndef _INTERFACE_H
#define _INTERFACE_H

#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include "ktypes.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* The following functions are inserted by region instrumentation pass */
void _KInit();
void _KDeinit();

void _KTurnOn();
void _KTurnOff();

void _KEnterRegion(UInt64 region_id, UInt region_type);
void _KExitRegion(UInt64 region_id, UInt region_type);

/* The following funcs are inserted by the critical path instrumentation pass */
void _KTimestamp(UInt32 dest_reg, UInt32 num_srcs, ...);
void _KTimestamp0(UInt32 dest_reg);
void _KTimestamp1(UInt32 dest_reg, UInt32 src_reg, UInt32 src_offset);
void _KTimestamp2(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset);

void _KWork(UInt32 work);

// BEGIN deprecated?
void _KInsertValue(UInt src, UInt dest); 
void _KInsertValueConst(UInt dest); 
// END deprecated?

void _KInduction(UInt dest_reg); 
void _KReduction(UInt op_cost, UInt dest_reg); 

void _KLoad(Addr src_addr, UInt dest, UInt32 mem_access_size, UInt32 num_srcs, ...); 
void _KLoad0(Addr src_addr, UInt dest, UInt32 size); 
void _KLoad1(Addr src_addr, UInt dest, UInt src1, UInt32 size);
void _KLoad2(Addr src_addr, UInt dest, UInt src1, UInt src2, UInt32 size);
void _KStore(UInt src, Addr dest_addr, UInt32 size); 
void _KStoreConst(Addr dest_addr, UInt32 size); 

void _KMalloc(Addr addr, size_t size, UInt dest);
void _KRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest);
void _KFree(Addr addr);

void _KPhi1To1(UInt dest, UInt src, UInt cd); 
void _KPhi2To1(UInt dest, UInt src, UInt cd1, UInt cd2); 
void _KPhi3To1(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3); 
void _KPhi4To1(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4); 
void _KPhiCond4To1(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4);
void _KPhiAddCond(UInt dest, UInt src);

void _KPushCDep(Reg cond);
void _KPopCDep();

void _KPrepCall(UInt64, UInt64);
void _KPrepRTable(UInt, UInt);
void _KLinkReturn(Reg dest);
void _KReturn(Reg src); 
void _KReturnConst(void);

void _KLinkArg(Reg src); 
void _KLinkArgConst(void);
void _KUnlinkArg(UInt dest); 

void _KCallLib(UInt cost, UInt dest, UInt num_in, ...); 

void _KPrintData(); // deprecated?


/* level management */
Level getMinReportLevel();
Level getMaxReportLevel();

// the following two functions are part of our plans for c++ support
void cppEntry();
void cppExit();

void _KPrepInvoke(UInt64);
void _KInvokeThrew(UInt64);
void _KInvokeOkay(UInt64);




#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */


#endif

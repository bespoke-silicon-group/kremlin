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

void _KEnterRegion(SID region_id, RegionType region_type);
void _KExitRegion(SID region_id, RegionType region_type);
void _KLandingPad(SID regionId, RegionType regionType);

/* The following funcs are inserted by the critical path instrumentation pass */
void _KTimestamp(UInt32 dest_reg, UInt32 num_srcs, ...);
void _KTimestamp0(UInt32 dest_reg);
void _KTimestamp1(UInt32 dest_reg, UInt32 src_reg, UInt32 src_offset);
void _KTimestamp2(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset);
void _KTimestamp3(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset);
void _KTimestamp4(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32 src4_reg, UInt32 src4_offset);
void _KTimestamp5(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset);
void _KTimestamp6(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32
src6_reg, UInt32 src6_offset);
void _KTimestamp7(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32
src6_reg, UInt32 src6_offset, UInt32 src7_reg, UInt32 src7_offset);

void _KWork(UInt32 work);

// BEGIN deprecated?
void _KInsertValue(UInt src, UInt dest); 
void _KInsertValueConst(UInt dest); 
// END deprecated?

void _KInduction(UInt dest_reg); 
void _KReduction(UInt op_cost, UInt dest_reg); 

// TODO: KLoads/Stores breaks the convention of having the dest followed by the src.
void _KLoad(Addr src_addr, Reg dest_reg, UInt32 mem_access_size, UInt32 num_srcs, ...); 
void _KLoad0(Addr src_addr, Reg dest_reg, UInt32 memory_access_size); 
void _KLoad1(Addr src_addr, Reg dest_reg, Reg src_reg, UInt32 memory_access_size);
void _KLoad2(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, UInt32 memory_access_size);
void _KLoad3(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, Reg src3_reg, UInt32 mem_access_size);
void _KLoad4(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, Reg src3_reg, Reg src4_reg, UInt32 mem_access_size);
void _KStore(Reg src_reg, Addr dest_addr, UInt32 memory_access_size); 
void _KStoreConst(Addr dest_addr, UInt32 memory_access_size); 

void _KPhi(Reg dest_reg, Reg src_reg, UInt32 num_ctrls, ...);
void _KPhi1To1(Reg dest_reg, Reg src_reg, Reg ctrl_reg); 
void _KPhi2To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg); 
void _KPhi3To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg); 
void _KPhi4To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg); 
void _KPhiCond4To1(Reg dest_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg);
void _KPhiAddCond(Reg dest_reg, Reg src_reg);

void _KMalloc(Addr addr, size_t size, UInt dest);
void _KRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest);
void _KFree(Addr addr);

void _KPushCDep(Reg cond);
void _KPopCDep();

void _KPrepCall(UInt64, UInt64);
void _KPrepRTable(UInt, UInt);
void _KLinkReturn(Reg dest);
void _KReturn(Reg src); 
void _KReturnConst(void);

void _KEnqArg(Reg src); 
void _KEnqArgConst(void);
void _KDeqArg(UInt dest); 

void _KCallLib(UInt cost, UInt dest, UInt num_in, ...); 

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

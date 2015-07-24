#ifndef __KREMLIN_API_HPP__
#define __KREMLIN_API_HPP__

#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

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
void _KTimestamp(uint32_t dest_reg, uint32_t num_srcs, ...);
void _KTimestamp0(uint32_t dest_reg);
void _KTimestamp1(uint32_t dest_reg, uint32_t src_reg, uint32_t src_offset);
void _KTimestamp2(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t src2_reg, uint32_t src2_offset);
void _KTimestamp3(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset);
void _KTimestamp4(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset, uint32_t src4_reg, uint32_t src4_offset);
void _KTimestamp5(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t
src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset, uint32_t
src4_reg, uint32_t src4_offset, uint32_t src5_reg, uint32_t src5_offset);
void _KTimestamp6(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t
src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset, uint32_t
src4_reg, uint32_t src4_offset, uint32_t src5_reg, uint32_t src5_offset, uint32_t
src6_reg, uint32_t src6_offset);
void _KTimestamp7(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t
src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset, uint32_t
src4_reg, uint32_t src4_offset, uint32_t src5_reg, uint32_t src5_offset, uint32_t
src6_reg, uint32_t src6_offset, uint32_t src7_reg, uint32_t src7_offset);

void _KWork(uint32_t work);

// BEGIN deprecated?
void _KInsertValue(uint32_t src, uint32_t dest); 
void _KInsertValueConst(uint32_t dest); 
// END deprecated?

void _KInduction(uint32_t dest_reg); 
void _KReduction(uint32_t op_cost, uint32_t dest_reg); 

// TODO: KLoads/Stores breaks the convention of having the dest followed by the src.
void _KLoad(Addr src_addr, Reg dest_reg, uint32_t mem_access_size, uint32_t num_srcs, ...); 
void _KLoad0(Addr src_addr, Reg dest_reg, uint32_t memory_access_size); 
void _KLoad1(Addr src_addr, Reg dest_reg, Reg src_reg, uint32_t memory_access_size);
void _KLoad2(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, uint32_t memory_access_size);
void _KLoad3(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, Reg src3_reg, uint32_t mem_access_size);
void _KLoad4(Addr src_addr, Reg dest_reg, Reg src1_reg, Reg src2_reg, Reg src3_reg, Reg src4_reg, uint32_t mem_access_size);
void _KStore(Reg src_reg, Addr dest_addr, uint32_t memory_access_size); 
void _KStoreConst(Addr dest_addr, uint32_t memory_access_size); 

void _KPhi(Reg dest_reg, Reg src_reg, uint32_t num_ctrls, ...);
void _KPhi1To1(Reg dest_reg, Reg src_reg, Reg ctrl_reg); 
void _KPhi2To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg); 
void _KPhi3To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg); 
void _KPhi4To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg); 
void _KPhiCond4To1(Reg dest_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg);
void _KPhiAddCond(Reg dest_reg, Reg src_reg);

void _KMalloc(Addr addr, size_t size, uint32_t dest);
void _KRealloc(Addr old_addr, Addr new_addr, size_t size, uint32_t dest);
void _KFree(Addr addr);

void _KPushCDep(Reg cond);
void _KPopCDep();

void _KPrepCall(uint64_t, uint64_t);
void _KPrepRTable(uint32_t, uint32_t);
void _KLinkReturn(Reg dest);
void _KReturn(Reg src); 
void _KReturnConst(void);

void _KEnqArg(Reg src); 
void _KEnqArgConst(void);
void _KDeqArg(uint32_t dest); 

void _KCallLib(uint32_t cost, uint32_t dest, uint32_t num_in, ...); 

// the following two functions are part of our plans for c++ support
void cppEntry();
void cppExit();

void _KPrepInvoke(uint64_t);
void _KInvokeThrew(uint64_t);
void _KInvokeOkay(uint64_t);




#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */


#endif

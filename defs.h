#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define TRUE 1
#define FALSE 0

// basic stuff needed
void addInit(unsigned int new_init);
void initProfiler();
void deinitProfiler();

void openOutputFile();
void closeOutputFile();

void linkInit(const void* cond);
void linkInitToCondition(const void* rhs, const void* lhs);

void logRegionEntry(unsigned int region_id, unsigned int region_type);
void logRegionExit(unsigned int region_id, unsigned int region_type);

unsigned int logBinaryOp(unsigned int id, unsigned int bb_id, int opcode, const void* arg1, const void* arg2, const void* address); 
unsigned int logBinaryOpConst(unsigned int id, unsigned int bb_id, int opcode, const void* arg, const void* address);
unsigned int logInductionVarDependence(const void* induct_var); 
unsigned int logAssignment(unsigned int id, unsigned int bb_id, const void* rhs, const void* lhs);
unsigned int logAssignmentConst(unsigned int id, unsigned int bb_id, const void* lhs); 
unsigned int logInsertValue(unsigned int op_id, unsigned int bb_id, const void* src_addr, const void* dst_addr); 
unsigned int logInsertValueConst(unsigned int op_id, unsigned int bb_id, const void* dst_addr); 


unsigned int logLibraryCall(unsigned int op_id, unsigned int bb_id, unsigned int cost, const void* out, unsigned int num_in, ...); 
unsigned int logMemOp(unsigned int op_id, unsigned int bb_id, const void* src_addr, const void* dst_addr, unsigned int is_store); 
void logPhiNode(unsigned int op_id, unsigned int bb_id, const void* dst_addr, unsigned int num_incoming_values, unsigned int num_t_inits, ...); 

unsigned int logOutputToConsole(unsigned int id, unsigned int bb_id, int num_out, ...); 
unsigned int logInputFromConsole(unsigned int id, unsigned int bb_id, int num_in, ...); 

void onBasicBlockEntry(unsigned int bb_id);


void stackVariableAlloc(unsigned int bb_id, const void* address); 
void stackVariableDealloc(unsigned int bb_id, const void* address); 

void linkAddrToArgName(unsigned int bb_id, const void* address, char* argname); 
void createArgLink(const void* address); 
void linkArgToAddr(unsigned int bb_id, const void* address); 
void linkArgToConst();
void transferAndUnlinkArg(unsigned int bb_id, unsigned int id, const void* a); 
void transferAndUnlinkArgName(unsigned int bb_id, const void* a, char* argname); 

void addReturnValueLink(void* address);

void onReturn(unsigned int op_id, unsigned int bb_id, const void* ret_val_addr); 

void setReturnTimestampConst();
void setReturnTimestamp(unsigned int bb_id, const void* retval);
void getReturnTimestamp(unsigned int id, unsigned int bb_id, const void* address);


void logBBVisit(unsigned int bb_id); 
void printProfileData(void);

void updateCriticalPathLength(int prospective_new_max_time, int node_id);

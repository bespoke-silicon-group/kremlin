#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define TRUE 1
#define FALSE 0

unsigned long long int counter = 0;

// basic stuff needed
void addInit(unsigned int new_init) {counter++;}
void removeInit() {counter++;}

void initProfiler() {counter++;}
void deinitProfiler() {counter++;}

void openOutputFile() {counter++;}
void closeOutputFile() {counter++;}

void printStats() {printf("counter is %llu\n",counter);}
void printProfileData(void) {counter++;}

void linkInit(const void* cond) {counter++;}
void linkInitToCondition(const void* rhs, const void* lhs) {counter++;}

void logRegionEntry(unsigned int region_id, unsigned int region_type) {counter++;}
void logRegionExit(unsigned int region_id, unsigned int region_type) {counter++;}


unsigned int logBinaryOp(unsigned int id, unsigned int bb_id, int opcode, const void* arg1, const void* arg2, const void* address) {counter++;}
unsigned int logBinaryOpConst(unsigned int id, unsigned int bb_id, int opcode, const void* arg, const void* address) {counter++;}
unsigned int logInductionVarDependence(const void* induct_var) {counter++;}
unsigned int logAssignment(unsigned int id, unsigned int bb_id, const void* rhs, const void* lhs) {counter++;}
unsigned int logAssignmentConst(unsigned int id, unsigned int bb_id, const void* lhs) {counter++;}
unsigned int logInsertValue(unsigned int op_id, unsigned int bb_id, const void* src_addr, const void* dst_addr) {counter++;}
unsigned int logInsertValueConst(unsigned int op_id, unsigned int bb_id, const void* dst_addr) {counter++;}


unsigned int logLibraryCall(unsigned int op_id, unsigned int bb_id, unsigned int cost, const void* out, unsigned int num_in, ...) {counter++;}
unsigned int logMemOp(unsigned int op_id, unsigned int bb_id, const void* src_addr, const void* dst_addr, unsigned int is_store) {counter++;}
void logPhiNode(unsigned int op_id, unsigned int bb_id, const void* dst_addr, unsigned int num_incoming_values, unsigned int num_t_inits, ...) {counter++;}

unsigned int logOutputToConsole(unsigned int id, unsigned int bb_id, int num_out, ...) {counter++;}
unsigned int logInputFromConsole(unsigned int id, unsigned int bb_id, int num_in, ...) {counter++;}



void stackVariableAlloc(unsigned int bb_id, const void* address) {counter++;}
void stackVariableDealloc(unsigned int bb_id, const void* address) {counter++;}

void linkAddrToArgName(unsigned int bb_id, const void* address, char* argname) {counter++;}
void createArgLink(const void* address) {counter++;}
void linkArgToAddr(unsigned int bb_id, const void* address) {counter++;}
void linkArgToConst() {counter++;}
void transferAndUnlinkArg(unsigned int bb_id, unsigned int id, const void* a) {counter++;}
void transferAndUnlinkArgName(unsigned int bb_id, const void* a, char* argname) {counter++;}

void addReturnValueLink(void* address) {counter++;}

void onReturn(unsigned int op_id, unsigned int bb_id, const void* ret_val_addr) {counter++;}

void setReturnTimestampConst() {counter++;}
void setReturnTimestamp(unsigned int bb_id, const void* retval) {counter++;}
void getReturnTimestamp(unsigned int id, unsigned int bb_id, const void* address) {counter++;}


void onBasicBlockEntry(unsigned int bb_id) {counter++;}
void logBBVisit(unsigned int bb_id) {counter++;}

void updateCriticalPathLength(int prospective_new_max_time, int node_id) {counter++;}

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define TRUE 1
#define FALSE 0

typedef unsigned long long int uint64_t;

unsigned long long int counter = 0;

unsigned long long int* table;

/*
typedef struct stack_frame {
	unsigned int current_bb_id;
	unsigned int last_bb_id;
} stack_frame;
*/

unsigned int current_bb_id;
unsigned int last_bb_id;

// basic stuff needed
void addInit(unsigned int new_init) {counter++;}
void removeInit() {counter++;}

void initProfiler() {
	table = (unsigned long long int*)malloc(1024*sizeof(unsigned long long int));
}

void deinitProfiler() {
	/*
	int i;
	for(i = 30; i < 40; ++i) {
		printf("%llu ",table[i]);
	}
	printf("\n");
	*/

	free(table);
}

void openOutputFile() {counter++;}
void closeOutputFile() {counter++;}

void printStats() {printf("counter is %llu\n",counter);}
void printProfileData(void) {counter++;}

void linkInit(const void* cond) {counter++;}
void linkInitToCondition(const void* rhs, const void* lhs) {counter++;}

void logRegionEntry(unsigned int region_id, unsigned int region_type) {counter++;}
void logRegionExit(unsigned int region_id, unsigned int region_type) {counter++;}


unsigned int logBinaryOp(unsigned int id, unsigned int bb_id, int opcode, const void* arg1, const void* arg2, const void* address) {
	unsigned long long int arg1_t, arg2_t, dest_t;
	arg1_t = table[(unsigned long long int)arg1 % 1024];
	arg2_t = table[(unsigned long long int)arg2 % 1024];

	if(arg1_t > arg2_t) {
		dest_t = arg1_t + 1;
	}
	else {
		dest_t = arg2_t + 1;
	}

	table[(unsigned long long int)address % 1024] = dest_t;

	return dest_t;
}

unsigned int logBinaryOpConst(unsigned int id, unsigned int bb_id, int opcode, const void* arg, const void* address) {
	unsigned long long int arg_t, dest_t;
	arg_t = table[(unsigned long long int)arg % 1024];

	dest_t = arg_t + 1;

	table[(unsigned long long int)address % 1024] = dest_t;

	return dest_t;
}

unsigned int logInductionVarDependence(const void* induct_var) {
	return 0;
}

unsigned int logAssignment(unsigned int id, unsigned int bb_id, const void* rhs, const void* lhs) {
	unsigned long long int rhs_t;
	rhs_t = table[(unsigned long long int)rhs % 1024];


	table[(unsigned long long int)lhs % 1024] = rhs_t;

	return rhs_t;
}

unsigned int logAssignmentConst(unsigned int id, unsigned int bb_id, const void* lhs) {
	table[(unsigned long long int)lhs % 1024] = 0;
	
	return 0;
}

unsigned int logInsertValue(unsigned int op_id, unsigned int bb_id, const void* src_addr, const void* dst_addr) {counter++;}
unsigned int logInsertValueConst(unsigned int op_id, unsigned int bb_id, const void* dst_addr) {counter++;}


unsigned int logLibraryCall(unsigned int op_id, unsigned int bb_id, unsigned int cost, const void* out, unsigned int num_in, ...) {counter++;}

unsigned int logMemOp(unsigned int op_id, unsigned int bb_id, const void* src_addr, const void* dst_addr, unsigned int is_store) {
	unsigned long long int src_t, dest_t;
	src_t = table[(unsigned long long int)src_addr % 1024];

	if(is_store) {
		dest_t = src_t + 1;
	}
	else {
		dest_t = src_t + 3;
	}

	table[(unsigned long long int)dst_addr % 1024] = dest_t;

	return dest_t;
}

void logPhiNode(unsigned int op_id, unsigned int bb_id, const void* dst_addr, unsigned int num_incoming_values, unsigned int num_t_inits, ...) {
	unsigned int incoming_bb_id;
	const void* incoming_addr;

	uint64_t curr_t;

	uint64_t max = 0;

	va_list ap;
	va_start(ap, num_t_inits);

	int i;
	for(i = 0; i < num_incoming_values; i++) {
		incoming_bb_id = va_arg(ap, unsigned int);
		incoming_addr = va_arg(ap, const void*);

		if(incoming_bb_id == last_bb_id) {
			curr_t = table[(uint64_t)incoming_addr % 1024];

			if(curr_t > max) max = curr_t;
		}
	}

	for(i = 0; i < num_t_inits; i++) {
		incoming_addr = va_arg(ap, const void*);
		curr_t = table[(uint64_t)incoming_addr % 1024];

		if(curr_t > max) max = curr_t;
	}

	va_end(ap);

	table[(unsigned long long int)dst_addr % 1024] = max;
}

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


void onBasicBlockEntry(unsigned int bb_id) {
	last_bb_id = current_bb_id;
	current_bb_id = bb_id;
}

void logBBVisit(unsigned int bb_id) {counter++;}

void updateCriticalPathLength(int prospective_new_max_time, int node_id) {counter++;}

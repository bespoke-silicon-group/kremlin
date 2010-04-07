#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "defs.h"



int foo() {
}

int getRegionLevel() {
	return 5;
}

UInt64 getCdt(int level) {
	return 0x1000;
}

UInt getVersion(int level) {
	static UInt version = 0;
	return version++;
}

void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest) {
	int level = getRegionLevel();
	int i = 0;
	TEntry* entry0 = getLTEntry(src0);
	TEntry* entry1 = getLTEntry(src1);
	TEntry* entryDest = getLTEntry(dest);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 ts1 = getTimestamp(entry1, i, version);
		UInt64 greater0 = (ts0 > ts1) ? ts0 : ts1;
		UInt64 greater1 = (cdt > greater0) ? cdt : greater0;
		updateTimestamp(entryDest, i, version, greater1 + opCost);
	}

	return NULL;
}


void* logBinaryOpConst(UInt opCost, UInt src, UInt dest) {
	int level = getRegionLevel();
	int i = 0;
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getLTEntry(dest);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		updateTimestamp(entryDest, i, version, greater1 + opCost);
	}

	return NULL;
}

void* logAssignment(UInt src, UInt dest) {
	int level = getRegionLevel();
	int i = 0;
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getLTEntry(dest);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		updateTimestamp(entryDest, i, version, greater1);
	}

	return NULL;

}

void* logAssignmentConst(UInt dest) {
	int level = getRegionLevel();
	int i = 0;
	TEntry* entryDest = getLTEntry(dest);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		updateTimestamp(entryDest, i, version, cdt);
	}

	return NULL;

}

void* logInsertValue(UInt src_addr, UInt dst_addr);
void* logInsertValueConst(UInt dst_addr);

void* logLoadInst(const void* src_addr, UInt dest);
void* logStoreInst(UInt src, const void* dest_addr);

void logPhiNode(UInt dest, UInt num_incoming_values, UInt num_t_inits, ...);

void addControlDep(UInt cond);
void removeControlDep();

void addReturnValueLink(UInt dest);
void logFuncReturn(UInt src);

void linkArgToLocal(UInt src);
void linkArgToConst();
void transferAndUnlinkArg(UInt dest);

UInt logLibraryCall(UInt cost, UInt dest, UInt num_in, ...);

void logBBVisit(UInt bb_id);


void initProfiler();
void deinitProfiler();

void logRegionEntry(UInt region_id, UInt region_type);
void logRegionExit(UInt region_id, UInt region_type);






/* The following functions are deprecated. Don't bother implementing them */
void linkInitToCondition(const void* rhs, const void* lhs);
UInt logInductionVarDependence(const void* induct_var);

UInt logOutputToConsole(UInt id, UInt bb_id, int num_out, ...);
UInt logInputFromConsole(UInt id, UInt bb_id, int num_in, ...);

void stackVariableAlloc(UInt bb_id, const void* address);
void stackVariableDealloc(UInt bb_id, const void* address);

void linkAddrToArgName(UInt bb_id, const void* address, char* argname);

void setReturnTimestampConst();
void setReturnTimestamp(UInt bb_id, const void* retval);
void getReturnTimestamp(UInt id, UInt bb_id, const void* address);

void onBasicBlockEntry(UInt bb_id);

void openOutputFile();
void closeOutputFile();

void linkInit(UInt cond);
void removeInit();

void printProfileData(void);


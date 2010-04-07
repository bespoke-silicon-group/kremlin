#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "defs.h"


#define MAX_REGION_LEVEL	100
static int regionLevel = -1;
static int versions[MAX_REGION_LEVEL];


static inline int getRegionLevel() {
	return regionLevel;
}

UInt64 getCdt(int level) {
	return 0x1000;
}

UInt getVersion(int level) {
	return versions[level];
}

void logRegionEntry(UInt region_id, UInt region_type) {
	regionLevel++;
	versions[regionLevel]++;
}


void logRegionExit(UInt region_id, UInt region_type) {
	regionLevel--;
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

	return entryDest;
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

	return entryDest;
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

	return entryDest;

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

	return entryDest;

}

void* logLoadInst(const void* src_addr, UInt dest) {
	int level = getRegionLevel();
	int i = 0;
	TEntry* entry0 = getGTEntry(src_addr);
	TEntry* entryDest = getLTEntry(dest);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		updateTimestamp(entryDest, i, version, greater1);
	}

	return entryDest;
}

void* logStoreInst(UInt src, const void* dest_addr) {
	int level = getRegionLevel();
	int i = 0;
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getGTEntry(dest_addr);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		updateTimestamp(entryDest, i, version, greater1);
	}

	return entryDest;
}


void* logInsertValue(UInt src, UInt dst) {
}
void* logInsertValueConst(UInt dst) {
}


void logPhiNode(UInt dest, UInt num_incoming_values, UInt num_t_inits, ...) {
	
}

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






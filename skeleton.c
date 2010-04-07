#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "defs.h"

#define MAX_ARGS 	10
typedef struct _CDT_T {
	TEntry* entry;
	struct _CDT_T* next;
} CDT;

typedef struct _FuncContext {
	LTable* table;
	TEntry* ret;
	TEntry* args[MAX_ARGS];
	int		writeIndex;
	int		readIndex;
	struct _FuncContext* next;
	
} FuncContext;



static int regionLevel = -1;
static int* versions;
static CDT* cdtHead;
static FuncContext* funcHead;

static FuncContext* pushFuncContext() {
	FuncContext* prevHead = funcHead;
	FuncContext* toAdd = (FuncContext*) malloc(sizeof(FuncContext()));
	toAdd->next = prevHead;
	funcHead = toAdd;
}

static FuncContext* popFuncContext() {
	FuncContext* ret = funcHead;
	funcHead = ret->next;
	return ret;
}

static inline int getRegionLevel() {
	return regionLevel;
}

UInt64 getCdt(int level) {
	return getTimestampNoVersion(cdtHead->entry, level);
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
	return logBinaryOpConst(0, src, dest);
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

void* logLoadInst(Addr src_addr, UInt dest) {
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

void* logStoreInst(UInt src, Addr dest_addr) {
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


void* logInsertValue(UInt src, UInt dest) {
	return logAssignment(src, dest);
}

void* logInsertValueConst(UInt dest) {
	return logAssignmentConst(dest);
}


void logPhiNode(UInt dest, UInt num_incoming_values, UInt num_t_inits, ...) {
	
}


void addControlDep(UInt cond) {
	TEntry* entry = getLTEntry(cond);
	CDT* toAdd = (CDT*) malloc(sizeof(CDT));
	toAdd->entry = entry;
	toAdd->next = cdtHead;
}

void removeControlDep() {
	CDT* toRemove = cdtHead;
	cdtHead = cdtHead->next;
	free(toRemove);
}


// prepare timestamp storage for return value
void addReturnValueLink(UInt dest) {
	pushFuncContext();
	funcHead->ret = getLTEntry(dest);
}

// write timestamp to the prepared storage
void logFuncReturn(UInt src) {
	TEntry* srcEntry = getLTEntry(src);
	copyTEntry(funcHead->ret, srcEntry);
}

// give timestamp for an arg
void linkArgToLocal(UInt src) {
	TEntry* srcEntry = getLTEntry(src);
	funcHead->args[funcHead->writeIndex++] = srcEntry;
}


TEntry* dummyEntry = NULL;
static void allocDummyTEntry() {
	dummyEntry = (TEntry*) allocTEntry(getMaxRegionLevel());
}
static TEntry* getDummyTEntry() {
	return dummyEntry;
}

static void freeDummyTEntry() {
	free(dummyEntry);
}

// special case for constant arg
void linkArgToConst() {
	funcHead->args[funcHead->writeIndex++] = getDummyTEntry();
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(UInt dest) {
	TEntry* destEntry = getLTEntry(dest);
	copyTEntry(destEntry, funcHead->args[funcHead->readIndex++]);
}


// use estimated cost for a callee function we cannot instrument
UInt logLibraryCall(UInt cost, UInt dest, UInt num_in, ...) { 
	
}

static UInt	prevBB;
static UInt	currentBB;

void logBBVisit(UInt bb_id) {
	prevBB = currentBB;
	currentBB = bb_id;
}


void initProfiler(int maxRegionLevel) {
	int maxVreg = 1000;
	initDataStructure(maxVreg, maxRegionLevel);
	versions = (int*) malloc(sizeof(int) * maxRegionLevel);
	allocDummyTEntry();
	pushFuncContext();
}

void deinitProfiler() {
	finalizeDataStructure();
	freeDummyTEntry();
	free(versions);
}



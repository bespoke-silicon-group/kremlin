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
	UInt64	work;
	struct _FuncContext* next;
	
} FuncContext;

static int 			regionNum = 0;
static int* 		versions = NULL;
static UInt64* 		cpLengths = NULL;
static CDT* 		cdtHead = NULL;
static FuncContext* funcHead = NULL;

static updateCP(TEntry* entry, int numLevel) {
	int i;
	for (i = 0; i < numLevel; i++) {
		UInt64 value = entry->time[i];
		if (value > cpLengths[i]) {
			cpLengths[i] = value;
		}
	}
}

static FuncContext* pushFuncContext() {
	int i;
	FuncContext* prevHead = funcHead;
	FuncContext* toAdd = (FuncContext*) malloc(sizeof(FuncContext));
	for (i = 0; i < MAX_ARGS; i++) {
		toAdd->args[i] = NULL;
	}
	toAdd->table = NULL;
	toAdd->next = prevHead;
	toAdd->writeIndex = 0;
	toAdd->readIndex = 0;
	toAdd->work = 0;
	funcHead = toAdd;
}

static inline void addWork(UInt work) {
	funcHead->work += work;	
}

static void popFuncContext() {
	FuncContext* ret = funcHead;
	funcHead = ret->next;
	if (funcHead != NULL)
		addWork(ret->work);
	freeLocalTable(ret->table);
	free(ret);	
}

static inline int getRegionNum() {
	return regionNum;
}

static inline int getCurrentRegion() {
	return regionNum - 1;
}



UInt64 getCdt(int level) {
	assert(cdtHead != NULL);
	return getTimestampNoVersion(cdtHead->entry, level);
}

UInt getVersion(int level) {
	return versions[level];
}

void setupLocalTable(UInt maxVregNum) {
	LTable* table = allocLocalTable(maxVregNum);
	assert(funcHead->table == NULL);
	funcHead->table = table;	
	setLocalTable(funcHead->table);
}

void prepareCall() {
	pushFuncContext();
}

void logRegionEntry(UInt region_id, UInt region_type) {
	regionNum++;
	int region = getCurrentRegion();
	versions[region]++;
	cpLengths[region] = 0;
}


void logRegionExit(UInt region_id, UInt region_type) {
	int i;
	int region = getCurrentRegion();
	UInt64 cpLength = cpLengths[region];
	printf("region %d level %d cp %d work %d\n", 
			region_id, region, cpLength, funcHead->work);

	if (region_type == RegionFunc) {
		popFuncContext();
		if (funcHead != NULL)
			setLocalTable(funcHead->table);
	}
	regionNum--;
}


void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest) {
	int level = getRegionNum();
	int i = 0;
	addWork(opCost);
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
	updateCP(entryDest, level);

	return entryDest;
}


void* logBinaryOpConst(UInt opCost, UInt src, UInt dest) {
	int level = getRegionNum();
	int i = 0;
	addWork(opCost);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getLTEntry(dest);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		updateTimestamp(entryDest, i, version, greater1 + opCost);
	}

	updateCP(entryDest, level);
	return entryDest;
}

void* logAssignment(UInt src, UInt dest) {
	return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
	int level = getRegionNum();
	int i = 0;
	TEntry* entryDest = getLTEntry(dest);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		updateTimestamp(entryDest, i, version, cdt);
	}

	updateCP(entryDest, level);
	return entryDest;

}

#define LOADCOST	10
#define STORECOST	10
void* logLoadInst(Addr src_addr, UInt dest) {
	int level = getRegionNum();
	int i = 0;
	addWork(LOADCOST);
	TEntry* entry0 = getGTEntry(src_addr);
	TEntry* entryDest = getLTEntry(dest);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		updateTimestamp(entryDest, i, version, greater1+LOADCOST);
	}

	updateCP(entryDest, level);
	return entryDest;
}

void* logStoreInst(UInt src, Addr dest_addr) {
	int level = getRegionNum();
	int i = 0;
	addWork(STORECOST);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getGTEntry(dest_addr);
	
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		updateTimestamp(entryDest, i, version, greater1+STORECOST);
	}

	updateCP(entryDest, level);
	return entryDest;
}


void* logInsertValue(UInt src, UInt dest) {
	return logAssignment(src, dest);
}

void* logInsertValueConst(UInt dest) {
	return logAssignmentConst(dest);
}


void addControlDep(UInt cond) {
	TEntry* entry = getLTEntry(cond);
	CDT* toAdd = (CDT*) malloc(sizeof(CDT));
	toAdd->entry = entry;
	toAdd->next = cdtHead;
	cdtHead = toAdd;
}

void removeControlDep() {
	CDT* toRemove = cdtHead;
	cdtHead = cdtHead->next;
	free(toRemove);
}


// prepare timestamp storage for return value
void addReturnValueLink(UInt dest) {
	funcHead->ret = getLTEntry(dest);
}

// write timestamp to the prepared storage
void logFuncReturn(UInt src) {
	TEntry* srcEntry = getLTEntry(src);
	assert(funcHead->ret != NULL);
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
	freeTEntry(dummyEntry);
}

// special case for constant arg
void linkArgToConst() {
	funcHead->args[funcHead->writeIndex++] = getDummyTEntry();
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(UInt dest) {
	TEntry* destEntry = getLTEntry(dest);
	assert(funcHead->args[funcHead->readIndex] != NULL);
	copyTEntry(destEntry, funcHead->args[funcHead->readIndex++]);
}


static UInt	prevBB;
static UInt	currentBB;

void logBBVisit(UInt bb_id) {
	prevBB = currentBB;
	currentBB = bb_id;
}

#define MAX_ENTRY 10

void logPhiNode(UInt dest, UInt num_incoming_values, UInt num_t_inits, ...) {
	TEntry* destEntry = getLTEntry(dest);
	TEntry* cdtEntry[MAX_ENTRY];
	TEntry* srcEntry = NULL;
	UInt	incomingBB[MAX_ENTRY];
	UInt	srcList[MAX_ENTRY];

	va_list ap;
	va_start(ap, num_t_inits);
	int level = getRegionNum();
	int i, j;
	
	// catch src dep
	for (i = 0; i < num_incoming_values; i++) { 
		incomingBB[i] = va_arg(ap, UInt);
		srcList[i] = va_arg(ap, UInt);
		if (incomingBB[i] == prevBB) {
			srcEntry = getLTEntry(srcList[i]);
		}
	}

	// read all CDT
	for (i = 0; i < num_t_inits; i++) {
		UInt cdt = va_arg(ap, UInt);
		cdtEntry[i] = getLTEntry(cdt);
	}
	va_end(ap);

	// get max
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 max = getTimestamp(srcEntry, i, version);
		
		for (j = 0; j < num_t_inits; j++) {
			UInt64 ts = getTimestamp(cdtEntry[j], i, version);
			if (ts > max)
				max = ts;		
		}
		updateTimestamp(destEntry, i, version, max);
	}
}


// use estimated cost for a callee function we cannot instrument
void* logLibraryCall(UInt cost, UInt dest, UInt num_in, ...) { 
	int i, j;
	int level = getRegionNum();
	TEntry* srcEntry[MAX_ENTRY];
	TEntry* destEntry = getLTEntry(dest);
	va_list ap;
	va_start(ap, num_in);

	for (i = 0; i < num_in; i++) {
		UInt src = va_arg(ap, UInt);
		srcEntry[i] = getLTEntry(src);
	}	
	va_end(ap);

	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 max = 0;
		
		for (j = 0; j < num_in; j++) {
			UInt64 ts = getTimestamp(srcEntry[j], i, version);
			if (ts > max)
				max = ts;
		}	
		
		updateTimestamp(destEntry, i, version, max + cost);
	}
	return destEntry;
	
}

void* logInductionVarDependence(UInt induct_var) {
}


void initProfiler() {
	int maxRegionLevel = MAX_REGION_LEVEL;
	initDataStructure(maxRegionLevel);
	versions = (int*) malloc(sizeof(int) * maxRegionLevel);
	bzero(versions, sizeof(int) * maxRegionLevel);

	cpLengths = (UInt64*) malloc(sizeof(UInt64) * maxRegionLevel);
	bzero(cpLengths, sizeof(UInt64) * maxRegionLevel);
	allocDummyTEntry();
	prepareCall();

}

void deinitProfiler() {
	finalizeDataStructure();
	freeDummyTEntry();
	free(cpLengths);
	free(versions);
}


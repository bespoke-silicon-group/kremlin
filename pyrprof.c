#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "defs.h"
#include "debug.h"
#include "table.h"

#define MAX_ARGS 		10


typedef struct _CDT_T {
	UInt64*	time;
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

typedef struct _cpLength {
	UInt64 start;
	UInt64 end;
} CPLength;

int 		regionNum = 0;
int* 		versions = NULL;
CPLength*	cpLengths = NULL;
CDT* 		cdtHead = NULL;
FuncContext* funcHead = NULL;
UInt64*		works = NULL;

void updateCP(UInt64 value, int level) {
	int i;
	if (value > cpLengths[level].end) {
		cpLengths[level].end = value;
	}
}

FuncContext* pushFuncContext() {
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

inline int getRegionNum() {
	return regionNum;
}

inline int getCurrentRegion() {
	return regionNum - 1;
}


inline void addWork(UInt work) {
	//funcHead->work += work;	
	int level = getCurrentRegion();
	works[level] += work;
}

void popFuncContext() {
	FuncContext* ret = funcHead;
	funcHead = ret->next;
	freeLocalTable(ret->table);
	free(ret);	
}

CDT* allocCDT() {
	CDT* ret = (CDT*) malloc(sizeof(CDT));
	ret->time = (UInt64*) malloc(sizeof(UInt64) * getMaxRegionLevel());
	bzero(ret->time, sizeof(UInt64) * getMaxRegionLevel());
	ret->next = NULL;
	return ret;
}

void fillCDT(CDT* cdt, TEntry* entry) {
	int numRegion = getRegionNum();
	int i;
	for (i = 0; i < numRegion; i++) {
		cdt->time[i] = entry->time[i];
	}
	UInt64 ts = cdt->time[i-1];
	for (; i < getMaxRegionLevel(); i++) {
		cdt->time[i] = ts;	
	}
}

void freeCDT(CDT* cdt) {
	free(cdt->time);
	free(cdt);	
}


UInt64 getCdt(int level) {
	assert(cdtHead != NULL);
	return cdtHead->time[level];
}

UInt getVersion(int level) {
	return versions[level];
}


inline UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version) {
    UInt64 ret = (entry->version[level] == version) ?
                    entry->time[level] : 0;
    return ret;
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
	works[region] = 0;
	MSG(0, "[+++] region %d level %d version %d\n", region_id, region, versions[region]);
	cpLengths[region].start = (region == 0) ? 0 : cpLengths[region-1].end;
	incIndentTab();
}


void logRegionExit(UInt region_id, UInt region_type) {
	int i;
	int region = getCurrentRegion();
	decIndentTab();
	UInt64 cpLength = cpLengths[region].end - cpLengths[region].start;
	MSG(0, "[---] region %d level %d cpStart %d cpEnd %d cp %d work %d\n", 
			region_id, region, cpLengths[region].start, cpLengths[region].end, 
			cpLength, works[region]);

	regionNum--;
	if (region_type == RegionFunc) {
		popFuncContext();
		if (funcHead != NULL)
			setLocalTable(funcHead->table);
	}
	if (regionNum > 0)
		addWork(works[region]);
}


void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest) {
	int level = getRegionNum();
	int i = 0;
	addWork(opCost);
	TEntry* entry0 = getLTEntry(src0);
	TEntry* entry1 = getLTEntry(src1);
	TEntry* entryDest = getLTEntry(dest);
	
	MSG(1, "binOp ts[%d] = max(ts[%d], ts[%d]) + %d\n", dest, src0, src1, opCost);
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 ts1 = getTimestamp(entry1, i, version);
		UInt64 greater0 = (ts0 > ts1) ? ts0 : ts1;
		UInt64 greater1 = (cdt > greater0) ? cdt : greater0;
		UInt64 value = greater1 + opCost;
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	MSG(2, "binOp[%d] level %d version %d work %d\n", opCost, i, version, works[i]);
	MSG(2, " src0 %d src1 %d dest %d\n", src0, src1, dest);
	MSG(2, " ts0 %d ts1 %d cdt %d value %d\n", ts0, ts1, cdt, value);
	}

	return entryDest;
}


void* logBinaryOpConst(UInt opCost, UInt src, UInt dest) {
	int level = getRegionNum();
	int i = 0;
	addWork(opCost);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getLTEntry(dest);
	
	MSG(1, "binOpConst ts[%d] = ts[%d] + %d\n", dest, src, opCost);
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		UInt64 value = greater1 + opCost;
		updateTimestamp(entryDest, i, version, greater1 + opCost);
		updateCP(value, i);
	MSG(2, "binOpConst[%d] level %d version %d work %d\n", opCost, i, version, works[i]);
	MSG(2, " src %d dest %d\n", src, dest);
	MSG(2, " ts0 %d cdt %d value %d\n", ts0, cdt, value);
	}

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
		updateCP(cdt, i);
	}

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
	
	MSG(1, "load ts[%d] = ts[0x%x] + %d\n", dest, src_addr, LOADCOST);
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		UInt64 value = greater1 + LOADCOST;
		updateTimestamp(entryDest, i, version, greater1+LOADCOST);
		updateCP(value, i);
	}

	return entryDest;
}

void* logStoreInst(UInt src, Addr dest_addr) {
	int level = getRegionNum();
	int i = 0;
	addWork(STORECOST);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getGTEntry(dest_addr);
	
	MSG(1, "store ts[0x%x] = ts[%d] + %d\n", dest_addr, src, STORECOST);
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		UInt64 value = greater1 + STORECOST;
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	}

	return entryDest;
}


void* logStoreInstConst(Addr dest_addr) {
	int level = getRegionNum();
	int i = 0;
	addWork(STORECOST);
	TEntry* entryDest = getGTEntry(dest_addr);
	
	MSG(1, "storeConst ts[0x%x] = %d\n", dest_addr, STORECOST);
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 value = cdt + STORECOST;
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	}

	return entryDest;
}

void* logInsertValue(UInt src, UInt dest) {
	printf("Warning: logInsertValue not correctly implemented\n");
	return logAssignment(src, dest);
}

void* logInsertValueConst(UInt dest) {
	printf("Warning: logInsertValueConst not correctly implemented\n");
	return logAssignmentConst(dest);
}


void addControlDep(UInt cond) {
	TEntry* entry = getLTEntry(cond);
	CDT* toAdd = allocCDT();
	fillCDT(toAdd, entry);
	toAdd->next = cdtHead;
	cdtHead = toAdd;
}

void removeControlDep() {
	CDT* toRemove = cdtHead;
	cdtHead = cdtHead->next;
	freeCDT(toRemove);
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

void logFuncReturnConst(void) {
	int i;
	int level = getRegionNum();
	for (i = 0; i < level; i++) {
		UInt64 cdt = getCdt(i);
		int version = getVersion(i);
		funcHead->ret->version[i] = version;
		funcHead->ret->time[i] = cdt;
	}
	
	
}

// give timestamp for an arg
void linkArgToLocal(UInt src) {
	TEntry* srcEntry = getLTEntry(src);
	funcHead->args[funcHead->writeIndex++] = srcEntry;
}


TEntry* dummyEntry = NULL;
void allocDummyTEntry() {
	dummyEntry = (TEntry*) allocTEntry(getMaxRegionLevel());
}

TEntry* getDummyTEntry() {
	return dummyEntry;
}

void freeDummyTEntry() {
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


UInt	prevBB;
UInt	currentBB;

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
	works = (UInt64*) malloc(sizeof(UInt64) * maxRegionLevel);
	bzero(versions, sizeof(int) * maxRegionLevel);
	bzero(works, sizeof(UInt64) * maxRegionLevel);

	cpLengths = (CPLength*) malloc(sizeof(CPLength) * maxRegionLevel);
	bzero(cpLengths, sizeof(CPLength) * maxRegionLevel);
	allocDummyTEntry();
	prepareCall();
	cdtHead = allocCDT();
}

void deinitProfiler() {
	finalizeDataStructure();
	freeDummyTEntry();
	free(cpLengths);
	free(versions);
	free(cdtHead);
	free(works);
	cdtHead = NULL;
}



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

#define MAX_ARGS 			10
#define MAX_STATIC_REGION	1000


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
	struct _FuncContext* next;
	
} FuncContext;

typedef struct _region_t {
	UInt64 start;
	UInt64 cp;
	UInt64 regionId;
} Region;

int 			regionNum = 0;
int* 			versions = NULL;
Region*			regionInfo = NULL;
CDT* 			cdtHead = NULL;
FuncContext* 	funcHead = NULL;
UInt64			timestamp = 0llu;
File* 			fp = NULL;
UInt64			dynamicRegionId[MAX_STATIC_REGION];	

#define getRegionNum() 		(regionNum)
#define getCurrentRegion() 	(regionNum-1)

void dumpRegion() {
#if 0
	int i;
	for (i = 0; i < getRegionNum(); i++) {
		fprintf(stderr, "%d: %lld\t%lld\n", 
			i, regionInfo[i].start, regionInfo[i].end);
	}
#endif
}

void updateCP(UInt64 value, int level) {
	if (value > regionInfo[level].cp) {
		regionInfo[level].cp = value;
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
	funcHead = toAdd;
//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);
}

void addWork(UInt work) {
	//funcHead->work += work;	
	//int level = getCurrentRegion();
	timestamp += work;
	//fprintf(stderr, "\tcurrent time = %llu\n", timestamp);
}

void popFuncContext() {
	FuncContext* ret = funcHead;
	funcHead = ret->next;
	FuncContext* next = (funcHead == NULL) ? NULL : ret->next;
//fprintf(stderr, "[pop ] head = 0x%x next = 0x%x\n", funcHead, next);
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
	assert(level >= 0);
	return cdtHead->time[level];
}

UInt getVersion(int level) {
	return versions[level];
}


UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version) {
	assert(entry != NULL);
	assert(level < MAX_REGION_LEVEL);
    UInt64 ret = (entry->version[level] == version) ?
                    entry->time[level] : 0;
    return ret;
}


void setupLocalTable(UInt maxVregNum) {
	MSG(0, "setupLocalTable size %u\n", maxVregNum);
	LTable* table = allocLocalTable(maxVregNum);
	assert(funcHead->table == NULL);
	funcHead->table = table;	
	setLocalTable(funcHead->table);
}

void prepareCall() {
	MSG(0, "prepareCall\n");
	pushFuncContext();
}

void logRegionEntry(UInt region_id, UInt region_type) {
	regionNum++;
	int region = getCurrentRegion();
	dynamicRegionId[region_id]++;
	versions[region]++;
	MSG(0, "[+++] type: %u region [%u, %u:%llu] start: %llu\n",
		region_type, region, region_id, dynamicRegionId[region_id], timestamp);
	regionInfo[region].regionId = region_id;
	regionInfo[region].start = timestamp;
	regionInfo[region].cp = 0;
	cdtHead->time[region] = 0;
	incIndentTab();
	dumpRegion();
}


void logRegionExit(UInt region_id, UInt region_type) {
	int i;
	int region = getCurrentRegion();

	UInt64 startTime = regionInfo[region].start;
	UInt64 endTime = timestamp;
	UInt64 work = endTime - regionInfo[region].start;
	UInt64 cp = regionInfo[region].cp;
	assert(region_id == regionInfo[region].regionId);
	UInt64 did = dynamicRegionId[region_id];
	UInt64 parentSid = (region > 1) ? regionInfo[region-1].regionId : 0;
	UInt64 parentDid = (region > 1) ? dynamicRegionId[parentSid] : 0;

	decIndentTab();
	MSG(0, "[---] type: %u region [%u, %u:%llu] parent [%llu:%llu] cp %llu work %llu\n",
			region_type, region, region_id, did, parentSid, parentDid, 
			regionInfo[region].cp, work);


	log_write(fp, region_id, 0, startTime, endTime, cp, parentSid, parentDid);

	dumpRegion();
	if (region_type == RegionFunc) { 
		popFuncContext();
		if (funcHead == NULL) {
			assert(getCurrentRegion() == 0);

		} else {
			setLocalTable(funcHead->table);
		}
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
	assert(funcHead->table->size > src0);
	assert(funcHead->table->size > src1);
	assert(funcHead->table->size > dest);
	assert(entry0 != NULL);
	assert(entry1 != NULL);
	assert(entryDest != NULL);
	
	MSG(1, "binOp ts[%u] = max(ts[%u], ts[%u]) + %u\n", dest, src0, src1, opCost);
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 ts1 = getTimestamp(entry1, i, version);
		UInt64 greater0 = (ts0 > ts1) ? ts0 : ts1;
		UInt64 greater1 = (cdt > greater0) ? cdt : greater0;
		UInt64 value = greater1 + opCost;
		assert(entryDest != NULL);
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	MSG(2, "binOp[%u] level %u version %u \n", opCost, i, version);
	MSG(2, " src0 %u src1 %u dest %u\n", src0, src1, dest);
	MSG(2, " ts0 %u ts1 %u cdt %u value %u\n", ts0, ts1, cdt, value);
	}

	dumpRegion();
	return entryDest;
}


void* logBinaryOpConst(UInt opCost, UInt src, UInt dest) {
	int level = getRegionNum();
	int i = 0;
	addWork(opCost);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getLTEntry(dest);
	
	assert(funcHead->table->size > src);
	assert(funcHead->table->size > dest);
	assert(entry0 != NULL);
	assert(entryDest != NULL);

	MSG(1, "binOpConst ts[%u] = ts[%u] + %u\n", dest, src, opCost);
	for (i = 0; i < level; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		UInt64 value = greater1 + opCost;
		updateTimestamp(entryDest, i, version, greater1 + opCost);
		updateCP(value, i);
	MSG(2, "binOpConst[%u] level %u version %u \n", opCost, i, version);
	MSG(2, " src %u dest %u\n", src, dest);
	MSG(2, " ts0 %u cdt %u value %u\n", ts0, cdt, value);
	}

	dumpRegion();
	return entryDest;
}

void* logAssignment(UInt src, UInt dest) {
	return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
	int level = getRegionNum();
	int i = 0;
	TEntry* entryDest = getLTEntry(dest);
	
	assert(funcHead->table->size > dest);
	assert(entryDest != NULL);

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
	MSG(1, "load ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOADCOST);
	addWork(LOADCOST);
	TEntry* entry0 = getGTEntry(src_addr);
	TEntry* entryDest = getLTEntry(dest);
	assert(funcHead->table->size > dest);
	assert(entryDest != NULL);
	assert(entry0 != NULL);
	
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
	
	assert(funcHead->table->size > src);
	assert(entryDest != NULL);
	assert(entry0 != NULL);

	MSG(1, "store ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORECOST);
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
	assert(entryDest != NULL);
	
	MSG(1, "storeConst ts[0x%x] = %u\n", dest_addr, STORECOST);
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
	MSG(1, "push ControlDep ts[%u]\n", cond);
	TEntry* entry = getLTEntry(cond);
	CDT* toAdd = allocCDT();
	fillCDT(toAdd, entry);
	toAdd->next = cdtHead;
	cdtHead = toAdd;
}

void removeControlDep() {
	MSG(1, "pop  ControlDep\n");
	CDT* toRemove = cdtHead;
	cdtHead = cdtHead->next;
	freeCDT(toRemove);
}


// prepare timestamp storage for return value
void addReturnValueLink(UInt dest) {
	MSG(1, "prepare return storage ts[%u]\n", dest);
	funcHead->ret = getLTEntry(dest);
}

// write timestamp to the prepared storage
void logFuncReturn(UInt src) {
	MSG(1, "write return value ts[%u]\n", src);
	TEntry* srcEntry = getLTEntry(src);
	assert(funcHead->ret != NULL);
	MSG(2, " entry = %s\n", toStringTEntry(srcEntry));
	copyTEntry(funcHead->ret, srcEntry);
}

void logFuncReturnConst(void) {
	int i;
	int level = getRegionNum();
	MSG(1, "logFuncReturnConst\n");
	for (i = 0; i < level; i++) {
		UInt64 cdt = getCdt(i);
		int version = getVersion(i);
		funcHead->ret->version[i] = version;
		funcHead->ret->time[i] = cdt;
	}
	
	
}

// give timestamp for an arg
void linkArgToLocal(UInt src) {
	MSG(1, "linkArgToLocal to ts[%u]\n", src);
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
	MSG(1, "linkArgToConst\n");
	funcHead->args[funcHead->writeIndex++] = getDummyTEntry();
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(UInt dest) {
	MSG(1, "getArgInfo to ts[%u]\n", dest);
	TEntry* destEntry = getLTEntry(dest);
	assert(funcHead->args[funcHead->readIndex] != NULL);
	copyTEntry(destEntry, funcHead->args[funcHead->readIndex++]);
}


UInt	__prevBB;
UInt	__currentBB;

void logBBVisit(UInt bb_id) {
	MSG(1, "logBBVisit(%u)\n", bb_id);
	__prevBB = __currentBB;
	__currentBB = bb_id;
}

#define MAX_ENTRY 10

void logPhiNode(UInt dest, UInt num_incoming_values, UInt num_t_inits, ...) {
	TEntry* destEntry = getLTEntry(dest);
	TEntry* cdtEntry[MAX_ENTRY];
	TEntry* srcEntry = NULL;
	UInt	incomingBB[MAX_ENTRY];
	UInt	srcList[MAX_ENTRY];

	MSG(1, "logPhiNode to ts[%u] from %u srcs\n", dest, num_incoming_values);
	va_list ap;
	va_start(ap, num_t_inits);
	int level = getRegionNum();
	int i, j;
	
	// catch src dep
	for (i = 0; i < num_incoming_values; i++) { 
		incomingBB[i] = va_arg(ap, UInt);
		srcList[i] = va_arg(ap, UInt);
		if (incomingBB[i] == __prevBB) {
			srcEntry = getLTEntry(srcList[i]);
			assert(srcEntry != NULL);
		}
	}
	if (srcEntry == NULL) {
		MSG(0, " actual prev = %d current = %d\n", __prevBB, __currentBB);
		for (i = 0; i < num_incoming_values; i++) {
			MSG(0, "\t phi prev[%d] = %d\n", i, incomingBB[i]);
			
		}
		assert(0);
	}

	// read all CDT
	for (i = 0; i < num_t_inits; i++) {
		UInt cdt = va_arg(ap, UInt);
		cdtEntry[i] = getLTEntry(cdt);
		assert(cdtEntry[i] != NULL);
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
	MSG(1, "logLibraryCall to ts[%u] with cost %u\n", dest, cost);
	int i, j;
	int level = getRegionNum();
	TEntry* srcEntry[MAX_ENTRY];
	TEntry* destEntry = getLTEntry(dest);
	va_list ap;
	va_start(ap, num_in);

	for (i = 0; i < num_in; i++) {
		UInt src = va_arg(ap, UInt);
		srcEntry[i] = getLTEntry(src);
		assert(srcEntry[i] != NULL);
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
	regionNum = 0;
	int maxRegionLevel = MAX_REGION_LEVEL;
	initDataStructure(maxRegionLevel);
	versions = (int*) malloc(sizeof(int) * maxRegionLevel);
	bzero(versions, sizeof(int) * maxRegionLevel);

	regionInfo = (Region*) malloc(sizeof(Region) * maxRegionLevel);
	bzero(regionInfo, sizeof(Region) * maxRegionLevel);
	allocDummyTEntry();
	prepareCall();
	cdtHead = allocCDT();
	
	fp = log_open("cpInfo.bin");
	assert(fp != NULL);
}

void deinitProfiler() {
	finalizeDataStructure();
	freeDummyTEntry();
	free(regionInfo);
	free(versions);
	freeCDT(cdtHead);
	cdtHead = NULL;
	log_close(fp);
}



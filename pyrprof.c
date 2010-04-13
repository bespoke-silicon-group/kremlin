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

#define MAX_ARGS 			20
#define MAX_STATIC_REGION	1000

#define MIN(a, b)	(((a) < (b)) ? (a) : (b))

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
#ifdef MANAGE_BB_INFO
	UInt	retBB;
	UInt	retPrevBB;
#endif
	struct _FuncContext* next;
	
} FuncContext;

typedef struct _region_t {
	UInt64 start;
	UInt64 cp;
	UInt64 regionId;
} Region;


#if 1
const int				_maxRegionToLog = MAX_REGION_LEVEL;
const int				_minRegionToLog = MIN_REGION_LEVEL;
#endif
#if 0
#define			_maxRegionToLog		5
#define			_minRegionToLog		0
#endif

int 			regionNum = 0;
int* 			versions = NULL;
Region*			regionInfo = NULL;
CDT* 			cdtHead = NULL;
FuncContext* 	funcHead = NULL;
UInt64			timestamp = 0llu;
File* 			fp = NULL;
UInt64*			dynamicRegionId[MAX_STATIC_REGION];	


#ifdef MANAGE_BB_INFO
UInt	__prevBB;
UInt	__currentBB;
#endif

#define getRegionNum() 		(regionNum)
#define getCurrentRegion() 	(regionNum-1)

void dumpCdt(CDT* cdt) {
	int i;
	fprintf(stderr, "cdtHead = 0x%x\n", cdt);
	for (i = 0; i < 5; i++) {
		fprintf(stderr, "\t%llu", cdt->time[i]);
	}
	fprintf(stderr, "\n");
}

void dumpTEntry(TEntry* entry) {
	int i;
	fprintf(stderr, "entry = 0x%x\n", entry);
	for (i = 0; i < 5; i++) {
		fprintf(stderr, "\t%llu", entry->time[i]);
	}
	fprintf(stderr, "\n");
}


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
#ifdef MANAGE_BB_INFO
	toAdd->retBB = __currentBB;
	toAdd->retPrevBB = __prevBB;
	MSG(0, "[push] current = %u last = %u\n", __currentBB, __prevBB);
#endif
	funcHead = toAdd;
//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);
}

void addWork(UInt work) {
	timestamp += work;
}

void popFuncContext() {
	FuncContext* ret = funcHead;
	// restore currentBB and prevBB
#ifdef MANAGE_BB_INFO
	__currentBB = ret->retBB;
	__prevBB = ret->retPrevBB;
#endif
	funcHead = ret->next;
	FuncContext* next = (funcHead == NULL) ? NULL : ret->next;
	freeLocalTable(ret->table);
	free(ret);	
}

UInt getVersion(int level) {
	return versions[level];
}


UInt64 getTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
	int level = inLevel - _minRegionToLog;
	assert(level >= 0);
	assert(entry != NULL);
	assert(level < _maxRegionToLog);
    UInt64 ret = (entry->version[level] == version) ?
                    entry->time[level] : 0;
    return ret;
}

void updateTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
	int level = inLevel - _minRegionToLog;
    assert(entry != NULL);
    entry->version[level] = version;
    entry->time[level] = timestamp;
}


CDT* allocCDT() {
	CDT* ret = (CDT*) malloc(sizeof(CDT));
	ret->time = (UInt64*) malloc(sizeof(UInt64) * getTEntrySize());
	bzero(ret->time, sizeof(UInt64) * getTEntrySize());
	ret->next = NULL;
	return ret;
}

void freeCDT(CDT* cdt) {
	free(cdt->time);
	free(cdt);	
}


UInt64 getCdt(int level) {
	assert(cdtHead != NULL);
	assert(level >= 0);
	return cdtHead->time[level - _minRegionToLog];
}

void setCdt(int level, UInt64 time) {
	assert(level >= _minRegionToLog);
	cdtHead->time[level - _minRegionToLog] = time;
}

void fillCDT(CDT* cdt, TEntry* entry) {
	int numRegion = getRegionNum();
	int i;
	for (i = _minRegionToLog; i <= _maxRegionToLog; i++) {
		cdt->time[i - _minRegionToLog] = entry->time[i - _minRegionToLog];
	}
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
	UInt64 parentSid = (region > 0) ? regionInfo[region-1].regionId : 0;
	UInt64 parentDid = (region > 0) ? dynamicRegionId[parentSid] : 0;
	MSG(0, "[+++] region [%u, %u, %u:%llu] parent [%u:%llu] start: %llu\n",
		region_type, region, region_id, dynamicRegionId[region_id], 
		parentSid, parentDid, timestamp);
	regionInfo[region].regionId = region_id;
	regionInfo[region].start = timestamp;
	regionInfo[region].cp = 0;
	if (region >= _minRegionToLog && region <= _maxRegionToLog)
		setCdt(region, 0);
	incIndentTab();
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
	UInt64 parentSid = (region > 0) ? regionInfo[region-1].regionId : 0;
	UInt64 parentDid = (region > 0) ? dynamicRegionId[parentSid] : 0;

	decIndentTab();
	MSG(0, "[---] region [%u, %u, %u:%llu] parent [%llu:%llu] cp %llu work %llu\n",
			region_type, region, region_id, did, parentSid, parentDid, 
			regionInfo[region].cp, work);


	log_write(fp, region_id, 0, startTime, endTime, cp, parentSid, parentDid);

	if (region_type == RegionFunc) { 
		popFuncContext();
		if (funcHead == NULL) {
			assert(getCurrentRegion() == 0);

		} else {
			setLocalTable(funcHead->table);
		}
#if MANAGE_BB_INFO
		MSG(0, "	currentBB: %u   lastBB: %u\n",
			__currentBB, __prevBB);
#endif
	}
	regionNum--;
}


void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest) {
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
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
	for (i = minLevel; i < maxLevel; i++) {
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

	return entryDest;
}


void* logBinaryOpConst(UInt opCost, UInt src, UInt dest) {
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	addWork(opCost);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getLTEntry(dest);
	
	assert(funcHead->table->size > src);
	assert(funcHead->table->size > dest);
	assert(entry0 != NULL);
	assert(entryDest != NULL);

	MSG(1, "binOpConst ts[%u] = ts[%u] + %u\n", dest, src, opCost);
	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		UInt64 value = greater1 + opCost;
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	MSG(2, "binOpConst[%u] level %u version %u \n", opCost, i, version);
	MSG(2, " src %u dest %u\n", src, dest);
	MSG(2, " ts0 %u cdt %u value %u\n", ts0, cdt, value);
	}

	return entryDest;
}

void* logAssignment(UInt src, UInt dest) {
	return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	TEntry* entryDest = getLTEntry(dest);
	
	assert(funcHead->table->size > dest);
	assert(entryDest != NULL);

	for (i = minLevel; i < maxLevel; i++) {
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
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	MSG(1, "load ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOADCOST);
	addWork(LOADCOST);
	TEntry* entry0 = getGTEntry(src_addr);
	TEntry* entryDest = getLTEntry(dest);
	assert(funcHead->table->size > dest);
	assert(entryDest != NULL);
	assert(entry0 != NULL);
	
	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		UInt64 value = greater1 + LOADCOST;
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	}

	return entryDest;
}

void* logStoreInst(UInt src, Addr dest_addr) {
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	addWork(STORECOST);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getGTEntry(dest_addr);
	
	assert(funcHead->table->size > src);
	assert(entryDest != NULL);
	assert(entry0 != NULL);

	MSG(1, "store ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORECOST);
	for (i = minLevel; i < maxLevel; i++) {
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
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	addWork(STORECOST);
	TEntry* entryDest = getGTEntry(dest_addr);
	assert(entryDest != NULL);
	
	MSG(1, "storeConst ts[0x%x] = %u\n", dest_addr, STORECOST);
	for (i = minLevel; i < maxLevel; i++) {
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
	copyTEntry(funcHead->ret, srcEntry);
}

void logFuncReturnConst(void) {
	int i;
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	MSG(1, "logFuncReturnConst\n");
	for (i = minLevel; i < maxLevel; i++) {
		UInt64 cdt = getCdt(i);
		int version = getVersion(i);
		updateTimestamp(funcHead->ret, i, version, cdt);
//		funcHead->ret->version[i] = version;
//		funcHead->ret->time[i] = cdt;
	}
	
	
}

// give timestamp for an arg
void linkArgToLocal(UInt src) {
	MSG(1, "linkArgToLocal to ts[%u]\n", src);
	TEntry* srcEntry = getLTEntry(src);
	assert(funcHead->writeIndex < MAX_ARGS);
	funcHead->args[funcHead->writeIndex++] = srcEntry;
}


TEntry* dummyEntry = NULL;
void allocDummyTEntry() {
	dummyEntry = (TEntry*) allocTEntry(getTEntrySize());
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
	assert(funcHead->writeIndex < MAX_ARGS);
	funcHead->args[funcHead->writeIndex++] = getDummyTEntry();
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(UInt dest) {
	MSG(1, "getArgInfo to ts[%u]\n", dest);
	TEntry* destEntry = getLTEntry(dest);
	assert(funcHead != NULL);
	assert(funcHead->args != NULL);
	assert(funcHead->args[funcHead->readIndex] != NULL);
	assert(funcHead->readIndex < funcHead->writeIndex);
	copyTEntry(destEntry, funcHead->args[funcHead->readIndex++]);
}

void logBBVisit(UInt bb_id) {
#ifdef MANAGE_BB_INFO
	MSG(1, "logBBVisit(%u)\n", bb_id);
	__prevBB = __currentBB;
	__currentBB = bb_id;
#endif
}

#define MAX_ENTRY 10

void logPhiNode(UInt dest, UInt src, UInt num_cont_dep, ...) {
	TEntry* destEntry = getLTEntry(dest);
	TEntry* cdtEntry[MAX_ENTRY];
	TEntry* srcEntry = NULL;
	UInt	incomingBB[MAX_ENTRY];
	UInt	srcList[MAX_ENTRY];

	MSG(1, "logPhiNode to ts[%u] from ts[%u] and %u control deps\n", dest, src, num_cont_dep);
	va_list ap;
	va_start(ap, num_cont_dep);
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	//int level = getRegionNum();
	int i, j;

	// catch src dep
	srcEntry = getLTEntry(src);

	// read all CDT
	for (i = 0; i < num_cont_dep; i++) {
		UInt cdt = va_arg(ap, UInt);
		cdtEntry[i] = getLTEntry(cdt);
		assert(cdtEntry[i] != NULL);
	}
	va_end(ap);

	// get max
	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 max = getTimestamp(srcEntry, i, version);
		
		for (j = 0; j < num_cont_dep; j++) {
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
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
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

	for (i = minLevel; i < maxLevel; i++) {
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
	int i;
	regionNum = 0;
	int storageSize = _maxRegionToLog - _minRegionToLog + 1;
	initDataStructure(storageSize);

	versions = (int*) malloc(sizeof(int) * _maxRegionToLog);
	bzero(versions, sizeof(int) * _maxRegionToLog);

	regionInfo = (Region*) malloc(sizeof(Region) * _maxRegionToLog);
	bzero(regionInfo, sizeof(Region) * _maxRegionToLog);
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



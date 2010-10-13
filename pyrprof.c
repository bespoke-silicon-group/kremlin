#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "defs.h"
#include "udr.h"
#include "log.h"
#include "debug.h"
#include "table.h"

#define _MAX_ARGS 			20
#define _MAX_REGION_LEVEL	100		// used for static data structures

#ifndef _MAX_STATIC_REGION_ID
#define _MAX_STATIC_REGION_ID	1000	// used for dynamic region id
#endif

#define MIN(a, b)	(((a) < (b)) ? (a) : (b))

typedef struct _CDT_T {
	UInt64*	time;
	UInt32* version;
	struct _CDT_T* next;
} CDT;

typedef struct _FuncContext {
	LTable* table;
	TEntry* ret;
	TEntry* args[_MAX_ARGS];
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
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 writeLineCnt;
} Region;

typedef struct _InvokeRecord {
	UInt id;
	int stackHeight;
} InvokeRecord;

#if 1
const int				_maxRegionToLog = MAX_REGION_LEVEL;
const int				_minRegionToLog = MIN_REGION_LEVEL;
#endif

int				pyrprofOn = 0;
int 			regionNum = 0;
int* 			versions = NULL;
Region*			regionInfo = NULL;
CDT* 			cdtHead = NULL;
FuncContext* 	funcHead = NULL;
InvokeRecord 	invokeStack[_MAX_REGION_LEVEL];
InvokeRecord*	invokeStackTop;
UInt64			timestamp = 0llu;
UInt64			loadCnt = 0llu;
UInt64			storeCnt = 0llu;
File* 			fp = NULL;
UInt64			dynamicRegionId[_MAX_STATIC_REGION_ID];	

#ifdef __cplusplus
int instrument = 0;
#endif

#ifdef MANAGE_BB_INFO
UInt	__prevBB;
UInt	__currentBB;
#endif

#define isPyrprofOn()		(pyrprofOn == 1)
#define getRegionNum() 		(regionNum)
#define getCurrentRegion() 	(regionNum-1)
#define isCurrentRegionInstrumentable() (((regionNum-1) >= _minRegionToLog) && ((regionNum-1) <= _maxRegionToLog))


/*
 * start profiling
 *
 * push the root region (id == 0, type == loop)
 * loop type seems weird, but using functino type as the root region
 * causes several problems regarding local table
 *
 * when pyrprofOn == 0,
 * most instrumentation functions do nothing.
 */ 
void turnOnProfiler() {
	pyrprofOn = 1;
	logRegionEntry(0, 1);
}

/*
 * end profiling
 *
 * pop the root region pushed in turnOnProfiler()
 */
void turnOffProfiler() {
	logRegionExit(0, 1);
	pyrprofOn = 0;
}


int _maxRegionNum = 0;
inline void incrementRegionLevel() {
	regionNum++;
	if (regionNum > _maxRegionNum)
		_maxRegionNum = regionNum;
}

inline void decrementRegionLevel() {
	regionNum--;
}
void dumpCdt(CDT* cdt) {
	int i;
	fprintf(stderr, "cdtHead = 0x%x\n", cdt);
	for (i = 0; i < 5; i++) {
		fprintf(stderr, "\t%llu", cdt->time[i]);
	}
	fprintf(stderr, "\n");
}

void dumpTEntry(TEntry* entry, int size) {
	int i;
	fprintf(stderr, "entry = 0x%x\n", entry);
	for (i = 0; i < size; i++) {
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

void incDynamicRegionId(UInt64 sid) {
	dynamicRegionId[sid]++;
	int i;
}

UInt64 getDynamicRegionId(UInt64 sid) {
	return dynamicRegionId[sid];
}

UInt64 getCP(int level) {
	return regionInfo[level].cp;
}

void updateCP(UInt64 value, int level) {
	if (value > regionInfo[level].cp) {
		regionInfo[level].cp = value;
	}

	//MSG(1,"CP[%d] = %llu\n",level,regionInfo[level].cp);
}

FuncContext* pushFuncContext() {
#ifdef __cplusplus
	instrument++;
#endif
	int i;
	FuncContext* prevHead = funcHead;
	FuncContext* toAdd = (FuncContext*) malloc(sizeof(FuncContext));
	for (i = 0; i < _MAX_ARGS; i++) {
		toAdd->args[i] = NULL;
	}
	toAdd->table = NULL;
	toAdd->next = prevHead;
	toAdd->writeIndex = 0;
	toAdd->readIndex = 0;
#ifdef MANAGE_BB_INFO
	toAdd->retBB = __currentBB;
	toAdd->retPrevBB = __prevBB;
	MSG(1, "[push] current = %u last = %u\n", __currentBB, __prevBB);
#endif
	funcHead = toAdd;
//fprintf(stderr, "[push] head = 0x%x next = 0x%x\n", funcHead, funcHead->next);
}

void addWork(UInt work) {
	timestamp += work;
}

void addLoad() {
	loadCnt++;
}

void addStore() {
	storeCnt++;
}

UInt64 _regionEntryCnt;
UInt64 _regionFuncCnt;
UInt64 _setupTableCnt;
int	_requireSetupTable;

void popFuncContext() {
#ifdef __cplusplus
	instrument--;
#endif
	FuncContext* ret = funcHead;
	assert(ret != NULL);
	//assert(ret->table != NULL);
	// restore currentBB and prevBB
#ifdef MANAGE_BB_INFO
	__currentBB = ret->retBB;
	__prevBB = ret->retPrevBB;
#endif
	funcHead = ret->next;
	FuncContext* next = (funcHead == NULL) ? NULL : ret->next;
	assert(_regionFuncCnt == _setupTableCnt);
	assert(_requireSetupTable == 0);
	assert(ret->table != NULL);
	if (ret->table != NULL)
		freeLocalTable(ret->table);
	free(ret);	
}

UInt getVersion(int level) {
	assert(level >= 0 && level < _MAX_REGION_LEVEL);
	return versions[level];
}


UInt64 getTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
	int level = inLevel - _minRegionToLog;
	assert(entry != NULL);
	assert(level >= 0 && level < getTEntrySize());
    UInt64 ret = (entry->version[level] == version) ?
                    entry->time[level] : 0;
    return ret;
}

UInt64 getReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
	int level = inLevel - _minRegionToLog;
	assert(entry != NULL);
	assert(level >= 0 && level < getTEntrySize());
    UInt64 ret = (entry->readVersion[level] == version) ?
                    entry->readTime[level] : 0;
    return ret;
}

void updateTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
	int level = inLevel - _minRegionToLog;
    assert(entry != NULL);

    entry->version[level] = version;
    entry->time[level] = timestamp;
}


void updateReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
	int level = inLevel - _minRegionToLog;
    assert(entry != NULL);

    entry->readVersion[level] = version;
    entry->readTime[level] = timestamp;
}

void updateReadMemoryAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
	int region = inLevel;
	UInt64 startTime = regionInfo[region].start;
	UInt64 prevTimestamp = getReadTimestamp(entry, inLevel, version);

	if (prevTimestamp == 0LL) {
		regionInfo[region].readCnt++;
		//fprintf(stderr, "\t[load] addr = 0x%x level = %d version = %d timestamp = %d\n",
		//	entry, inLevel, version, timestamp);

		updateReadTimestamp(entry, inLevel, version, timestamp);
	}
}
	
// 1) update readCnt and writeCnt
// 2) update timestamp
void updateWriteMemoryAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
	int region = inLevel;
	UInt64 startTime = regionInfo[region].start;
	UInt64 prevTimestamp = getTimestamp(entry, inLevel, version);
	if (prevTimestamp == 0LL) {
		regionInfo[region].writeCnt++;
	} 	
	//updateTimestamp(entry, inLevel, version, timestamp);
}

void updateReadMemoryLineAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
	int region = inLevel;
	UInt64 startTime = regionInfo[region].start;
	UInt64 prevTimestamp = getReadTimestamp(entry, inLevel, version);
	if (prevTimestamp == 0LL) {
		regionInfo[region].readLineCnt++;
		//fprintf(stderr, "[line] addr = 0x%x level = %d version = %d timestamp = %d\n",
		//	entry, inLevel, version, timestamp);
		updateReadTimestamp(entry, inLevel, version, timestamp);

	}
}

void updateWriteMemoryLineAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
	int region = inLevel;
	UInt64 startTime = regionInfo[region].start;
	UInt64 prevTimestamp = getTimestamp(entry, inLevel, version);
	if (prevTimestamp == 0LL) {
		regionInfo[region].writeLineCnt++;
	}
	updateTimestamp(entry, inLevel, version, timestamp);
}

CDT* allocCDT() {
	CDT* ret = (CDT*) malloc(sizeof(CDT));
	ret->time = (UInt64*) malloc(sizeof(UInt64) * getTEntrySize());
	ret->version = (UInt32*) malloc(sizeof(UInt32) * getTEntrySize());
	bzero(ret->time, sizeof(UInt64) * getTEntrySize());
	bzero(ret->version, sizeof(UInt32) * getTEntrySize());
	ret->next = NULL;
	return ret;
}

void freeCDT(CDT* cdt) {
	free(cdt->time);
	free(cdt->version);
	free(cdt);	
}


UInt64 getCdt(int level, UInt32 version) {
	assert(cdtHead != NULL);
	assert(level >= 0);
	return (cdtHead->version[level - _minRegionToLog] == version) ?
		cdtHead->time[level - _minRegionToLog] : 0;
}

void setCdt(int level, UInt32 version, UInt64 time) {
	assert(level >= _minRegionToLog);
	cdtHead->time[level - _minRegionToLog] = time;
	cdtHead->version[level - _minRegionToLog] = version;
}

void fillCDT(CDT* cdt, TEntry* entry) {
	int numRegion = getRegionNum();
	int i;
	for (i = _minRegionToLog; i <= _maxRegionToLog; i++) {
		cdt->time[i - _minRegionToLog] = entry->time[i - _minRegionToLog];
		cdt->version[i - _minRegionToLog] = entry->version[i - _minRegionToLog];
	}
}

void setupLocalTable(UInt maxVregNum) {
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "setupLocalTable size %u\n", maxVregNum);

#ifndef WORK_ONLY
	assert(_requireSetupTable == 1);
	LTable* table = allocLocalTable(maxVregNum);
	assert(funcHead->table == NULL);
	assert(table != NULL);
	funcHead->table = table;	
	setLocalTable(funcHead->table);
	_setupTableCnt++;
	_requireSetupTable = 0;
#endif
}

#ifdef __cplusplus
void invokeAssert() {
	assert(invokeStackTop >= invokeStack);
	assert(invokeStackTop < invokeStack + _MAX_REGION_LEVEL);
}

void prepareInvoke(UInt id) {
	if(!instrument)
		return;
	MSG(1, "prepareInvoke(%u) - saved at %d\n", id, instrument);

	invokeAssert();

	InvokeRecord* currentRecord = invokeStackTop++;
	currentRecord->id = id;
	currentRecord->stackHeight = instrument;

	invokeAssert();
}

void invokeOkay(UInt id) {
	if(!instrument)
		return;

	invokeAssert();
	if(invokeStackTop > invokeStack && (invokeStackTop - 1)->id == id) {
		MSG(1, "invokeOkay(%u)\n", id);
		invokeStackTop--;

		invokeAssert();
	} else
		MSG(1, "invokeOkay(%u) ignored\n", id);
}
void invokeThrew(UInt id)
{
	if(!instrument)
		return;

	invokeAssert();

	if(invokeStackTop > invokeStack && (invokeStackTop - 1)->id == id) {
		InvokeRecord* currentRecord = invokeStackTop - 1;
		MSG(1, "invokeThrew(%u) - Popping to %d\n", currentRecord->id, currentRecord->stackHeight);

		int lastInstrument = instrument;
		while(instrument > currentRecord->stackHeight)
		{
			int region = getCurrentRegion();
			logRegionExit(regionInfo[region].regionId, 0);

			assert(instrument < lastInstrument);
			lastInstrument = instrument;
		}
		invokeStackTop--;
		
		invokeAssert();
	}
	else
		MSG(1, "invokeThrew(%u) ignored\n", id);
}
#endif

void prepareCall() {
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "prepareCall\n");
#ifndef WORK_ONLY
	pushFuncContext();
	_requireSetupTable = 1;
#endif
}



void logRegionEntry(UInt region_id, UInt region_type) {
	if (region_type == 0)
		_regionFuncCnt++;

	if (!isPyrprofOn()) {
		return;
	}

#ifdef __cplusplus
	if(!instrument)
		return;
#endif

	incrementRegionLevel();
	_regionEntryCnt++;
	int region = getCurrentRegion();
/*
	if (region < _minRegionToLog || region > _maxRegionToLog)
		return;
*/
	incDynamicRegionId(region_id);
	versions[region]++;
	UInt64 parentSid = (region > 0) ? regionInfo[region-1].regionId : 0;
	UInt64 parentDid = (region > 0) ? getDynamicRegionId(parentSid) : 0;
	if (region_type < 2)
		MSG(0, "[+++] region [%u, %d, %u:%llu] parent [%llu:%llu] start: %llu\n",
			region_type, region, region_id, getDynamicRegionId(region_id), 
			parentSid, parentDid, timestamp);
	regionInfo[region].regionId = region_id;
	regionInfo[region].start = timestamp;
	regionInfo[region].cp = 0;
	regionInfo[region].readCnt = 0;
	regionInfo[region].writeCnt = 0;
	regionInfo[region].readLineCnt = 0;
	regionInfo[region].writeLineCnt = 0;
#ifndef WORK_ONLY
	if (region >= _minRegionToLog && region <= _maxRegionToLog)
		setCdt(region, versions[region], 0);
#endif
	incIndentTab();
}

UInt64 _lastSid;
UInt64 _lastDid;
UInt64 _lastWork;
UInt64 _lastCP;
UInt64 _lastStart;
UInt64 _lastEnd;
UInt64 _lastCnt;
UInt64 _lastParentSid;
UInt64 _lastParentDid;

void logRegionExit(UInt region_id, UInt region_type) {
	if (!isPyrprofOn()) {
		if (region_type == RegionFunc) { 
			popFuncContext();
			if (funcHead == NULL) {
				assert(getCurrentRegion() <= 0);
	
			} else {
				setLocalTable(funcHead->table);
			}
		}
		return;
	}
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	int i;
	int region = getCurrentRegion();

	UInt64 sid = (UInt64)region_id;
	UInt64 startTime = regionInfo[region].start;
	UInt64 endTime = timestamp;
	UInt64 work = endTime - regionInfo[region].start;
	UInt64 cp = regionInfo[region].cp;

	if(region_id != regionInfo[region].regionId) {
		fprintf(stderr,"ERROR: unexpected region exit: %u (expected region %u)\n",region_id,regionInfo[region].regionId);
		assert(0);
	}
	UInt64 did = getDynamicRegionId(region_id);
	UInt64 parentSid = (region > 0) ? regionInfo[region-1].regionId : 0;
	UInt64 parentDid = (region > 0) ? getDynamicRegionId(parentSid) : 0;

	if(work < cp) {
		fprintf(stderr,"ERROR: cp (%llu) > work (%llu) [region_id=%u]",cp,work,region_id);
		assert(0);
	}

	decIndentTab();
	if (region_type < 2)
		MSG(0, "[---] region [%u, %u, %u:%llu] parent [%llu:%llu] cp %llu work %llu\n",
				region_type, region, region_id, did, parentSid, parentDid, 
				regionInfo[region].cp, work);
	if (isPyrprofOn() && work > 0 && cp == 0 && isCurrentRegionInstrumentable()) {
		fprintf(stderr, "cp should be a non-zero number when work is non-zero\n");
		fprintf(stderr, "region [type: %u, level: %u, id: %u:%llu] parent [%llu:%llu] cp %llu work %llu\n",
			region_type, region, region_id, did, parentSid, parentDid, 
			regionInfo[region].cp, work);
		assert(0);
	}

#ifdef USE_UREGION
	URegionField field;
	field.work = work;
	field.cp = cp;
	field.readCnt = regionInfo[region].readCnt;
	field.writeCnt = regionInfo[region].writeCnt;
	field.readLineCnt = regionInfo[region].readLineCnt;
	field.writeLineCnt = regionInfo[region].writeLineCnt;

	assert(work >= field.readCnt && work >= field.writeCnt);
	processUdr(sid, did, parentSid, parentDid, field);
#else
	if (_lastWork == work &&
		_lastCP == cp &&
		_lastCnt > 0 &&
		_lastSid == sid ) {
		_lastCnt++;

	} else {
		if (_lastCnt > 0)
			log_write(fp, _lastSid, _lastDid, _lastStart, _lastEnd, _lastCP, _lastParentSid, _lastParentDid, _lastCnt);
		_lastSid = sid;
		_lastDid = did;
		_lastWork = work;
		_lastCP = cp;
		_lastCnt = 1;		
		_lastStart = startTime;
		_lastEnd = endTime;
		_lastParentSid = parentSid;
		_lastParentDid = parentDid;
	}
#endif
		
#ifndef WORK_ONLY
	if (region_type == RegionFunc) { 
		popFuncContext();
		if (funcHead == NULL) {
			assert(getCurrentRegion() == 0);

		} else {
			setLocalTable(funcHead->table);
		}
#if MANAGE_BB_INFO
		MSG(1, "	currentBB: %u   lastBB: %u\n",
			__currentBB, __prevBB);
#endif
	}
#endif
	decrementRegionLevel();
}

// TODO: implement me
void logLoopIteration() {}

void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest) {
	if (!isPyrprofOn())
		return NULL;

#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "binOp ts[%u] = max(ts[%u], ts[%u]) + %u\n", dest, src0, src1, opCost);
	addWork(opCost);

#ifndef WORK_ONLY
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	TEntry* entry0 = getLTEntry(src0);
	TEntry* entry1 = getLTEntry(src1);
	TEntry* entryDest = getLTEntry(dest);
	assert(funcHead->table->size > src0);
	assert(funcHead->table->size > src1);
	assert(funcHead->table->size > dest);
	assert(entry0 != NULL);
	assert(entry1 != NULL);
	assert(entryDest != NULL);
	
	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i,version);
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
#else
	return NULL;
#endif
}


void* logBinaryOpConst(UInt opCost, UInt src, UInt dest) {
	if (!isPyrprofOn())
		return NULL;

#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "binOpConst ts[%u] = ts[%u] + %u\n", dest, src, opCost);
	addWork(opCost);

#ifndef WORK_ONLY
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	assert(funcHead->table != NULL);
	assert(funcHead->table->size > src);
	assert(funcHead->table->size > dest);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getLTEntry(dest);
	
	assert(entry0 != NULL);
	assert(entryDest != NULL);

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i,version);
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
#else
	return NULL;
#endif
}

void* logAssignment(UInt src, UInt dest) {
	if (!isPyrprofOn())
		return NULL;
	
#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	return logBinaryOpConst(0, src, dest);
}

void* logAssignmentConst(UInt dest) {
#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "logAssignmentConst ts[%u]\n", dest);

#ifndef WORK_ONLY
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	TEntry* entryDest = getLTEntry(dest);
	
	assert(funcHead->table->size > dest);
	assert(entryDest != NULL);

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i,version);
		updateTimestamp(entryDest, i, version, cdt);
		updateCP(cdt, i);
	}

	return entryDest;
#else
	return NULL;
#endif
}

void* logLoadInst(Addr src_addr, UInt dest) {
	if (!isPyrprofOn())
		return NULL;

#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "load ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOAD_COST);
	addWork(LOAD_COST);
	addLoad();

#ifndef WORK_ONLY
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	TEntry* entry0 = getGTEntry(src_addr);
	TEntry* entry0Line = getGTEntryCacheLine(src_addr);
	TEntry* entryDest = getLTEntry(dest);
	assert(funcHead->table->size > dest);
	assert(entryDest != NULL);
	assert(entry0 != NULL);
	
	//fprintf(stderr, "\n\nload ts[%u] = ts[0x%x] + %u\n", dest, src_addr, LOAD_COST);
	//fprintf(stderr, "load addr = 0x%x, entryLine = 0x%x\n", src_addr, entry0Line);
	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i,version);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		UInt64 value = greater1 + LOAD_COST;
		updateReadMemoryAccess(entry0, i, version, value);
		updateReadMemoryLineAccess(entry0Line, i, version, value);
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	}

	return entryDest;
#else
	return NULL;
#endif
}

void* logStoreInst(UInt src, Addr dest_addr) {
	if (!isPyrprofOn())
		return NULL;
#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "store ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);
	addWork(STORE_COST);
	addStore();

#ifndef WORK_ONLY
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	TEntry* entry0 = getLTEntry(src);
	TEntry* entryDest = getGTEntry(dest_addr);
	TEntry* entryLine = getGTEntryCacheLine(dest_addr);
	assert(funcHead->table->size > src);
	assert(entryDest != NULL);
	assert(entry0 != NULL);

	//fprintf(stderr, "\n\nstore ts[0x%x] = ts[%u] + %u\n", dest_addr, src, STORE_COST);
	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i,version);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 greater1 = (cdt > ts0) ? cdt : ts0;
		UInt64 value = greater1 + STORE_COST;
		updateWriteMemoryAccess(entryDest, i, version, value);
		updateWriteMemoryLineAccess(entryLine, i, version, value);
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	}

	return entryDest;
#else
	return NULL;
#endif
}


void* logStoreInstConst(Addr dest_addr) {
	if (!isPyrprofOn())
		return NULL;

#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "storeConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
	addWork(STORE_COST);

#ifndef WORK_ONLY
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	TEntry* entryDest = getGTEntry(dest_addr);
	TEntry* entryLine = getGTEntryCacheLine(dest_addr);
	assert(entryDest != NULL);
	
	//fprintf(stderr, "\nstoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
	for (i = minLevel; i < maxLevel; ++i) {
		UInt version = getVersion(i);
		UInt64 cdt = getCdt(i,version);
		UInt64 value = cdt + STORE_COST;
		updateWriteMemoryAccess(entryDest, i, version, value);
		updateWriteMemoryLineAccess(entryLine, i, version, value);
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
	}

	return entryDest;
#else
	return NULL;
#endif
}

// TODO: 64 bit?
void logMalloc(Addr addr, size_t size) {
	if (!isPyrprofOn())
		return;
	
	MSG(1, "logMalloc addr=0x%x size=%llu\n", addr, (UInt64)size);

#ifndef WORK_ONLY
	assert(size != 0);

	UInt32 start_index, end_index;

	start_index = ((UInt64) addr >> 16) & 0xffff;

	UInt64 end_addr = (UInt64)addr + (size-1);

	end_index = (end_addr >> 16) & 0xffff;

	assert(start_index < 0x10000 && end_index < 0x10000);

	//MSG(1,"start_index = %lu, end_index = %lu\n",start_index,end_index);

	if(start_index == end_index) {
		// get/create entry and set "used" field appropriately
		GEntry* entry = gTable->array[start_index];
		if (entry == NULL) {
			entry = createGEntry();
			gTable->array[start_index] = entry;
			entry->used = (size >> 2);
		}
		else {
			entry->used += (size >> 2);
		}

		MSG(2,"  allocating from gTable[%lu]. %hu used in this block\n",start_index,entry->used);

		UInt32 start_index2, end_index2;

		// find starting and ending addr
		start_index2 = ((UInt64) addr >> 2) & 0x3fff;
		end_index2 = (end_addr >> 2) & 0x3fff;

		MSG(2,"  gTable entry range: [%lu:%lu]\n",start_index2,end_index2);

		// create TEntry instances for the range of mem addrs
		// We assume that TEntry instances don't exist for the
		// index2 range because otherwise malloc would be buggy.
		int i;
		for(i = start_index2; i <= end_index2; ++i) {
			entry->array[i] = allocTEntry(maxRegionLevel);
		}
	}
	else {
		// handle start_index
		GEntry* entry = gTable->array[start_index];
		UInt32 start_index2 = ((UInt64) addr >> 2) & 0x3fff;

		if (entry == NULL) {
			entry = createGEntry();
			gTable->array[start_index] = entry;
			entry->used = (0x4000-start_index2);
		}
		else {
			entry->used += (0x4000-start_index2);
		}

		MSG(2,"  allocating from gTable[%lu]. %hu used in this block\n",start_index,entry->used);

		int i;
		for(i = start_index2; i < 0x4000; ++i) {
			entry->array[i] = allocTEntry(maxRegionLevel);
		}

		// handle end_index
		entry = gTable->array[end_index];
		UInt32 end_index2 = (end_addr >> 2) & 0x3fff;

		if (entry == NULL) {
			entry = createGEntry();
			gTable->array[end_index] = entry;
			entry->used = end_index2 + 1;
		}
		else {
			entry->used += (end_index2+1);
		}

		for(i = 0; i <= end_index2; ++i) {
			entry->array[i] = allocTEntry(maxRegionLevel);
		}

		MSG(2,"  allocating from gTable[%lu]. %hu used in this block\n",end_index,entry->used);

		// handle all intermediate indices
		UInt32 curr_index;
		for(curr_index = start_index+1; curr_index < end_index; ++curr_index) {
			// assume that malloc isn't buggy and therefore won't give us addresses
			// that have been used but not freed
			entry = createGEntry();
			gTable->array[curr_index] = entry;
			entry->used = 0x4000;

			MSG(2,"  allocating all entries in gTable[%lu].\n",curr_index);

			for(i = 0; i < 0x4000; ++i) {
				entry->array[i] = allocTEntry(maxRegionLevel);
			}
		}
	}

	createMEntry(addr,size);
#endif
}

void logFree(Addr addr) {
	if (!isPyrprofOn())
		return;
	MSG(1, "logFree addr=0x%x\n", addr);

#ifndef WORK_ONLY
	MEntry* me = getMEntry(addr);

	size_t mem_size = me->size;

	UInt32 start_index, end_index;

	start_index = ((UInt64) addr >> 16) & 0xffff;

	UInt64 end_addr = (UInt64)addr + (mem_size-1);

	end_index = (end_addr >> 16) & 0xffff;

	if(start_index == end_index) {
		// get entry (must exist b/c of logMalloc)
		GEntry* entry = gTable->array[start_index];
		assert(entry != NULL);

		entry->used -= (mem_size >> 2);

		MSG(2,"  freeing from gTable[%lu]. %hu entries remain in use\n",start_index,entry->used);

		UInt32 start_index2, end_index2;

		// find starting and ending addr
		start_index2 = ((UInt64) addr >> 2) & 0x3fff;
		end_index2 = (end_addr >> 2) & 0x3fff;

		MSG(2,"  gTable entry range: [%lu:%lu]\n",start_index2,end_index2);

		// free TEntry instances for the range of mem addrs
		int i;
		for(i = start_index2; i <= end_index2; ++i) {
			freeTEntry(entry->array[i]);
			entry->array[i] = NULL;
		}

		// if nothing in this gtable entry is used
		// then we can safely free it
		if(entry->used == 0) { 
			free(entry);
			gTable->array[start_index] = NULL;
			MSG(2,"    freeing gTable entry.\n");
		}
	}
	else {
		// handle start_index
		GEntry* entry = gTable->array[start_index];
		UInt32 start_index2 = ((UInt64) addr >> 2) & 0x3fff;

		int i;
		for(i = start_index2; i < 0x4000; ++i) {
			freeTEntry(entry->array[i]);
			entry->array[i] = NULL;
		}

		entry->used -= (0x4000-start_index2);

		MSG(2,"  freeing from gTable[%lu] (range: [%lu:0x4000]). %hu entries remain in use\n",start_index,start_index2,entry->used);

		// free it if nothing used
		if(entry->used == 0) { 
			free(entry);
			gTable->array[start_index] = NULL;
			MSG(2,"    freeing gTable entry.\n");
		}

		// handle end_index
		entry = gTable->array[end_index];
		UInt32 end_index2 = (end_addr >> 2) & 0x3fff;

		for(i = 0; i <= end_index2; ++i) {
			freeTEntry(entry->array[i]);
			entry->array[i] = NULL;
		}

		entry->used -= (end_index2+1);

		MSG(2,"  freeing from gTable[%lu] (range: [0:%lu]). %hu entries remain in use\n",end_index,entry->used);

		// free it if nothing used
		if(entry->used == 0) { 
			free(entry);
			gTable->array[end_index] = NULL;
			MSG(2,"    freeing gTable entry.\n");
		}

		// handle all intermediate indices
		UInt32 curr_index;
		for(curr_index = start_index+1; curr_index < end_index; ++curr_index) {
			entry = gTable->array[curr_index];

			MSG(2,"  freeing all entries from gTable[%lu].\n",curr_index);

			for(i = 0; i < 0x4000; ++i) {
				freeTEntry(entry->array[i]);
				// we don't need to set entry->array[i] to NULL here since we are
				// nullifying the whole entry
			}

			// intermediate will always have all TEntries
			// deleted and therefore are always safe to free
			free(entry);
			gTable->array[curr_index] = NULL;
		}
	}

	freeMEntry(addr);
#endif
}

// TODO: more efficient implementation (if old_addr = new_addr)
void logRealloc(Addr old_addr, Addr new_addr, size_t size) {
	if (!isPyrprofOn())
		return;

	MSG(1, "logRealloc old_addr=0x%x new_addr=0x%x size=%llu\n", old_addr, new_addr, (UInt64)size);
	logFree(old_addr);
	logMalloc(new_addr,size);
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
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(2, "push ControlDep ts[%u]\n", cond);

#ifndef WORK_ONLY
	CDT* toAdd = allocCDT();
	if (isPyrprofOn()) {
		TEntry* entry = getLTEntry(cond);
		fillCDT(toAdd, entry);
	}
	toAdd->next = cdtHead;
	cdtHead = toAdd;
#endif
}

void removeControlDep() {
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(2, "pop  ControlDep\n");

#ifndef WORK_ONLY
	CDT* toRemove = cdtHead;
	cdtHead = cdtHead->next;
	freeCDT(toRemove);
#endif
}


// prepare timestamp storage for return value
void addReturnValueLink(UInt dest) {
	if (!isPyrprofOn())
		return;
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "prepare return storage ts[%u]\n", dest);
#ifndef WORK_ONLY
	funcHead->ret = getLTEntry(dest);
#endif
}

// write timestamp to the prepared storage
void logFuncReturn(UInt src) {
	if (!isPyrprofOn())
		return;
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "write return value ts[%u]\n", src);

#ifndef WORK_ONLY
	TEntry* srcEntry = getLTEntry(src);
	assert(funcHead->ret != NULL);
	copyTEntry(funcHead->ret, srcEntry);
#endif
}

void logFuncReturnConst(void) {
	if (!isPyrprofOn())
		return;
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "logFuncReturnConst\n");

#ifndef WORK_ONLY
	int i;
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	for (i = minLevel; i < maxLevel; i++) {
		int version = getVersion(i);
		UInt64 cdt = getCdt(i,version);
		updateTimestamp(funcHead->ret, i, version, cdt);
//		funcHead->ret->version[i] = version;
//		funcHead->ret->time[i] = cdt;
	}
#endif
}

// give timestamp for an arg
void linkArgToLocal(UInt src) {
	if (!isPyrprofOn())
		return;
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "linkArgToLocal to ts[%u]\n", src);

#ifndef WORK_ONLY
	TEntry* srcEntry = getLTEntry(src);
	assert(funcHead->writeIndex < _MAX_ARGS);
	funcHead->args[funcHead->writeIndex++] = srcEntry;
#endif
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
	dummyEntry = NULL;
}

// special case for constant arg
void linkArgToConst() {
	if (!isPyrprofOn())
		return;
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "linkArgToConst\n");

#ifndef WORK_ONLY
	assert(funcHead->writeIndex < _MAX_ARGS);
	funcHead->args[funcHead->writeIndex++] = getDummyTEntry();
#endif
}

// get timestamp for an arg and associate it with a local vreg
// should be called in the order of linkArgToLocal
void transferAndUnlinkArg(UInt dest) {
	if (!isPyrprofOn())
		return;
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "getArgInfo to ts[%u]\n", dest);

#ifndef WORK_ONLY
	TEntry* destEntry = getLTEntry(dest);
	assert(funcHead != NULL);
	assert(funcHead->args != NULL);
	assert(funcHead->args[funcHead->readIndex] != NULL);
	assert(funcHead->readIndex < funcHead->writeIndex);
	copyTEntry(destEntry, funcHead->args[funcHead->readIndex++]);
#endif
}

void logBBVisit(UInt bb_id) {
	if (!isPyrprofOn())
		return;
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
#ifdef MANAGE_BB_INFO
	MSG(1, "logBBVisit(%u)\n", bb_id);
	__prevBB = __currentBB;
	__currentBB = bb_id;
#endif
}

void* logPhiNode1CD(UInt dest, UInt src, UInt cd) {
	if (!isPyrprofOn())
		return NULL;
#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "logPhiNode1CD ts[%u] = max(ts[%u], ts[%u])\n", dest, src, cd);

#ifndef WORK_ONLY
	TEntry* entrySrc = getLTEntry(src);
	TEntry* entryCD = getLTEntry(cd);
	TEntry* entryDest = getLTEntry(dest);

	assert(funcHead->table->size > src);
	assert(funcHead->table->size > cd);
	assert(funcHead->table->size > dest);
	assert(entrySrc != NULL);
	assert(entryCD != NULL);
	assert(entryDest != NULL);
	
	int i = 0;
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 ts_src = getTimestamp(entrySrc, i, version);
		UInt64 ts_cd = getTimestamp(entryCD, i, version);
		UInt64 max = (ts_src > ts_cd) ? ts_src : ts_cd;
		updateTimestamp(entryDest, i, version, max);
		MSG(2, "logPhiNode1CD level %u version %u \n", i, version);
		MSG(2, " src %u cd %u dest %u\n", src, cd, dest);
		MSG(2, " ts_src %u ts_cd %u max %u\n", ts_src, ts_cd, max);
	}

	return entryDest;
#else
	return NULL;
#endif
}

void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2) {
	if (!isPyrprofOn())
		return NULL;

#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif

	MSG(1, "logPhiNode2CD ts[%u] = max(ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2);

#ifndef WORK_ONLY
	TEntry* entrySrc = getLTEntry(src);
	TEntry* entryCD1 = getLTEntry(cd1);
	TEntry* entryCD2 = getLTEntry(cd2);
	TEntry* entryDest = getLTEntry(dest);

	assert(funcHead->table->size > src);
	assert(funcHead->table->size > cd1);
	assert(funcHead->table->size > cd2);
	assert(funcHead->table->size > dest);
	assert(entrySrc != NULL);
	assert(entryCD1 != NULL);
	assert(entryCD2 != NULL);
	assert(entryDest != NULL);
	
	int i = 0;
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 ts_src = getTimestamp(entrySrc, i, version);
		UInt64 ts_cd1 = getTimestamp(entryCD1, i, version);
		UInt64 ts_cd2 = getTimestamp(entryCD2, i, version);
		UInt64 max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
		UInt64 max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
		updateTimestamp(entryDest, i, version, max2);
		MSG(2, "logPhiNode2CD level %u version %u \n", i, version);
		MSG(2, " src %u cd1 %u cd2 %u dest %u\n", src, cd1, cd2, dest);
		MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u max %u\n", ts_src, ts_cd1, ts_cd2, max2);
	}

	return entryDest;
#else
	return NULL;
#endif
}

void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3) {
	if (!isPyrprofOn())
		return NULL;
#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "logPhiNode3CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3);

#ifndef WORK_ONLY
	TEntry* entrySrc = getLTEntry(src);
	TEntry* entryCD1 = getLTEntry(cd1);
	TEntry* entryCD2 = getLTEntry(cd2);
	TEntry* entryCD3 = getLTEntry(cd3);
	TEntry* entryDest = getLTEntry(dest);

	assert(funcHead->table->size > src);
	assert(funcHead->table->size > cd1);
	assert(funcHead->table->size > cd2);
	assert(funcHead->table->size > cd3);
	assert(funcHead->table->size > dest);
	assert(entrySrc != NULL);
	assert(entryCD1 != NULL);
	assert(entryCD2 != NULL);
	assert(entryCD3 != NULL);
	assert(entryDest != NULL);
	
	int i = 0;
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 ts_src = getTimestamp(entrySrc, i, version);
		UInt64 ts_cd1 = getTimestamp(entryCD1, i, version);
		UInt64 ts_cd2 = getTimestamp(entryCD2, i, version);
		UInt64 ts_cd3 = getTimestamp(entryCD3, i, version);
		UInt64 max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
		UInt64 max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
		UInt64 max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
		updateTimestamp(entryDest, i, version, max3);
		MSG(2, "logPhiNode3CD level %u version %u \n", i, version);
		MSG(2, " src %u cd1 %u cd2 %u cd3 %u dest %u\n", src, cd1, cd2, cd3, dest);
		MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u max %u\n", ts_src, ts_cd1, ts_cd2, ts_cd3, max3);
	}

	return entryDest;
#else
	return NULL;
#endif
}

void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
	if (!isPyrprofOn())
		return NULL;

#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "logPhiNode4CD ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, src, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
	TEntry* entrySrc = getLTEntry(src);
	TEntry* entryCD1 = getLTEntry(cd1);
	TEntry* entryCD2 = getLTEntry(cd2);
	TEntry* entryCD3 = getLTEntry(cd3);
	TEntry* entryCD4 = getLTEntry(cd4);
	TEntry* entryDest = getLTEntry(dest);

	assert(funcHead->table->size > src);
	assert(funcHead->table->size > cd1);
	assert(funcHead->table->size > cd2);
	assert(funcHead->table->size > cd3);
	assert(funcHead->table->size > cd4);
	assert(funcHead->table->size > dest);
	assert(entrySrc != NULL);
	assert(entryCD1 != NULL);
	assert(entryCD2 != NULL);
	assert(entryCD3 != NULL);
	assert(entryCD4 != NULL);
	assert(entryDest != NULL);
	
	int i = 0;
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 ts_src = getTimestamp(entrySrc, i, version);
		UInt64 ts_cd1 = getTimestamp(entryCD1, i, version);
		UInt64 ts_cd2 = getTimestamp(entryCD2, i, version);
		UInt64 ts_cd3 = getTimestamp(entryCD3, i, version);
		UInt64 ts_cd4 = getTimestamp(entryCD4, i, version);
		UInt64 max1 = (ts_src > ts_cd1) ? ts_src : ts_cd1;
		UInt64 max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
		UInt64 max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
		UInt64 max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
		updateTimestamp(entryDest, i, version, max4);
		MSG(2, "logPhiNode4CD level %u version %u \n", i, version);
		MSG(2, " src %u cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", src, cd1, cd2, cd3, cd4, dest);
		MSG(2, " ts_src %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", ts_src, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
	}

	return entryDest;
#else
	return NULL;
#endif
}

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {
	if (!isPyrprofOn())
		return NULL;

#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "log4CDToPhiNode ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", dest, dest, cd1, cd2, cd3, cd4);

#ifndef WORK_ONLY
	TEntry* entryCD1 = getLTEntry(cd1);
	TEntry* entryCD2 = getLTEntry(cd2);
	TEntry* entryCD3 = getLTEntry(cd3);
	TEntry* entryCD4 = getLTEntry(cd4);
	TEntry* entryDest = getLTEntry(dest);

	assert(funcHead->table->size > cd1);
	assert(funcHead->table->size > cd2);
	assert(funcHead->table->size > cd3);
	assert(funcHead->table->size > cd4);
	assert(funcHead->table->size > dest);
	assert(entryCD1 != NULL);
	assert(entryCD2 != NULL);
	assert(entryCD3 != NULL);
	assert(entryCD4 != NULL);
	assert(entryDest != NULL);
	
	int i = 0;
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 ts_dest = getTimestamp(entryDest, i, version);
		UInt64 ts_cd1 = getTimestamp(entryCD1, i, version);
		UInt64 ts_cd2 = getTimestamp(entryCD2, i, version);
		UInt64 ts_cd3 = getTimestamp(entryCD3, i, version);
		UInt64 ts_cd4 = getTimestamp(entryCD4, i, version);
		UInt64 max1 = (ts_dest > ts_cd1) ? ts_dest : ts_cd1;
		UInt64 max2 = (max1 > ts_cd2) ? max1 : ts_cd2;
		UInt64 max3 = (max2 > ts_cd3) ? max2 : ts_cd3;
		UInt64 max4 = (max3 > ts_cd4) ? max3 : ts_cd4;
		updateTimestamp(entryDest, i, version, max4);
		MSG(2, "log4CDToPhiNode4CD level %u version %u \n", i, version);
		MSG(2, " cd1 %u cd2 %u cd3 %u cd4 %u dest %u\n", cd1, cd2, cd3, cd4, dest);
		MSG(2, " ts_dest %u ts_cd1 %u ts_cd2 %u ts_cd3 %u ts_cd4 %u max %u\n", ts_dest, ts_cd1, ts_cd2, ts_cd3, ts_cd4, max4);
	}

	return entryDest;
#else
	return NULL;
#endif
}

#define MAX_ENTRY 10

void logPhiNodeAddCondition(UInt dest, UInt src) {
	if (!isPyrprofOn())
		return;
#ifdef __cplusplus
	if(!instrument)
		return;
#endif
	MSG(1, "logPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest, src, dest);

#ifndef WORK_ONLY
	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());
	int i = 0;
	assert(funcHead->table != NULL);
	assert(funcHead->table->size > src);
	assert(funcHead->table->size > dest);
	TEntry* entry0 = getLTEntry(src);
	TEntry* entry1 = getLTEntry(dest);
	TEntry* entryDest = entry1;
	
	assert(entry0 != NULL);
	assert(entry1 != NULL);
	assert(entryDest != NULL);

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 ts0 = getTimestamp(entry0, i, version);
		UInt64 ts1 = getTimestamp(entry1, i, version);
		UInt64 value = (ts0 > ts1) ? ts0 : ts1;
		updateTimestamp(entryDest, i, version, value);
		updateCP(value, i);
		MSG(2, "logPhiAddCond level %u version %u \n", i, version);
		MSG(2, " src %u dest %u\n", src, dest);
		MSG(2, " ts0 %u ts1 %u value %u\n", ts0, ts1, value);
	}
#endif
}

// use estimated cost for a callee function we cannot instrument
void* logLibraryCall(UInt cost, UInt dest, UInt num_in, ...) { 
	if (!isPyrprofOn())
		return NULL;

#ifdef __cplusplus
	if(!instrument)
		return NULL;
#endif
	MSG(1, "logLibraryCall to ts[%u] with cost %u\n", dest, cost);
	addWork(cost);

#ifndef WORK_ONLY
	TEntry* srcEntry[MAX_ENTRY];
	TEntry* destEntry = getLTEntry(dest);

	va_list ap;
	va_start(ap, num_in);

	int i;
	for (i = 0; i < num_in; i++) {
		UInt src = va_arg(ap, UInt);
		srcEntry[i] = getLTEntry(src);
		assert(srcEntry[i] != NULL);
	}	
	va_end(ap);

	int minLevel = _minRegionToLog;
	int maxLevel = MIN(_maxRegionToLog+1, getRegionNum());

	for (i = minLevel; i < maxLevel; i++) {
		UInt version = getVersion(i);
		UInt64 max = 0;
		
		int j;
		for (j = 0; j < num_in; j++) {
			UInt64 ts = getTimestamp(srcEntry[j], i, version);
			if (ts > max)
				max = ts;
		}	
		
		updateTimestamp(destEntry, i, version, max + cost);
	}
	return destEntry;
#else
	return NULL;
#endif
	
}

// this function is the same as logAssignmentConst but helps to quickly
// identify induction variables in the source code
void* logInductionVar(UInt dest) {
	if (!isPyrprofOn())
		return NULL;
	return logAssignmentConst(dest);
}

void* logInductionVarDependence(UInt induct_var) {
}

void* logReductionVar(UInt opCost, UInt dest) {
	addWork(opCost);
}

UInt isCpp = FALSE;
UInt hasInitialized = 0;

int pyrprof_init() {
	if(hasInitialized++) {
		MSG(0, "pyrprof_init skipped\n");
		return FALSE;
	}
	MSG(0, "pyrprof_init running\n");

	pyrprofOn = TRUE;

	int alky = _MAX_STATIC_REGION_ID;
	fprintf(stderr,"number of static regions: %d\n",alky);
	fprintf(stderr,"DEBUGLEVEL = %d\n", DEBUGLEVEL);

#ifdef __cplusplus
	instrument++;
#endif

	int i;
	regionNum = 0;
	invokeStackTop = invokeStack;
	int storageSize = _maxRegionToLog - _minRegionToLog + 1;
	MSG(0, "minLevel = %d maxLevel = %d storageSize = %d\n", 
		_minRegionToLog, _maxRegionToLog, storageSize);
	initDataStructure(storageSize);

	assert(versions = (int*) malloc(sizeof(int) * _MAX_REGION_LEVEL));
	bzero(versions, sizeof(int) * _MAX_REGION_LEVEL);

	assert(regionInfo = (Region*) malloc(sizeof(Region) * _MAX_REGION_LEVEL));
	bzero(regionInfo, sizeof(Region) * _MAX_REGION_LEVEL);
	allocDummyTEntry();
	prepareCall();
	cdtHead = allocCDT();
	
	fp = log_open("cpInfo.bin");
	assert(fp != NULL);

	return TRUE;
}

int pyrprof_deinit() {
	if(--hasInitialized) {
		MSG(0, "pyrprof_deinit skipped\n");
		return FALSE;
	}
	MSG(0, "pyrprof_deinit running\n");

#ifdef USE_UREGION
	finalizeUdr();
#else
	assert(_lastCnt == 1 && _lastParentSid == 0);
	log_write(fp, _lastSid, _lastDid, _lastStart, _lastEnd, _lastCP, _lastParentSid, _lastParentDid, _lastCnt);
#endif

	finalizeDataStructure();

	freeDummyTEntry();

	free(regionInfo);
	regionInfo = NULL;

	free(versions);
	versions = NULL;

	freeCDT(cdtHead);
	cdtHead = NULL;

#ifdef __cplusplus
	instrument--;
#endif

	log_close(fp);

	fprintf(stderr, "[pyrprof] minRegionLevel = %d maxRegionLevel = %d\n", 
		_minRegionToLog, _maxRegionToLog);
	fprintf(stderr, "[pyrprof] app MaxRegionLevel = %d\n", _maxRegionNum);

	pyrprofOn = FALSE;

	return TRUE;
}

void initProfiler() {
	pyrprof_init();
}

void cppEntry() {
	isCpp = TRUE;
	pyrprof_init();
}

void cppExit() {
	pyrprof_deinit();
}

void deinitProfiler() {
	pyrprof_deinit();
}



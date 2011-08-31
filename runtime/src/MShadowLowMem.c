#include "defs.h"

#ifndef USE_MSHADOW_BASE

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "debug.h"
#include "hash_map.h"
#include "CRegion.h"
#include "Vector.h"
#include "MShadow.h"
#include "MemAlloc.h"
#include "Table.h"

#define USE_VERSION_TABLE
//#define BYPASS_CACHE


// Mask of the L1 table.
#define L1_MASK 0xfffff

// Size of the L1 table.
#define L1_SIZE (L1_MASK + 1)

// Bits to shift right before the mask
#define L1_SHIFT 12
#define L2_MASK 0x3ff

//#define L1_SHIFT 16
//#define L2_MASK 0x3fff

// Mask of the L2 table.

// The size of the L2 table. Since only word addressible, shift by 2
#define L2_SIZE (L2_MASK + 1)
#define L2_SHIFT 2

/*
 * MemStat
 */
typedef struct _MemStat {
	int nSTableEntry;

	int nSegTableAllocated;
	int nSegTableActive;
	int nSegTableActiveMax;

	int nTimeTableAllocated;
	int nTimeTableFreed;
	int nTimeTableActive;
	int nTimeTableActiveMax;

} MemStat;

static MemStat stat;

void printMemStat() {
	fprintf(stderr, "nSTableEntry = %d\n\n", stat.nSTableEntry);

	fprintf(stderr, "nSegTableAllocated = %d\n", stat.nSegTableAllocated);
	fprintf(stderr, "nSegTableActive = %d\n", stat.nSegTableActive);
	fprintf(stderr, "nSegTableActiveMax = %d\n\n", stat.nSegTableActiveMax);

	fprintf(stderr, "nTimeTableAllocated = %d\n", stat.nTimeTableAllocated);
	fprintf(stderr, "nTimeTableFreed = %d\n", stat.nTimeTableFreed);
	fprintf(stderr, "nTimeTableActive = %d\n", stat.nTimeTableActive);
	fprintf(stderr, "nTimeTableActiveMax = %d\n\n", stat.nTimeTableActiveMax);
}


/*
 * CacheStat
 */

typedef struct _L1Stat {
	int nRead;
	int nReadHit;
	int nReadEvict;
	int nWrite;
	int nWriteHit;
	int nWriteEvict;
} L1Stat;

L1Stat cacheStat;

static inline void eventRead() {
	cacheStat.nRead++;
}

static inline void eventReadHit() {
	cacheStat.nReadHit++;
}

static inline void eventReadEvict() {
	cacheStat.nReadEvict++;
}

static inline void eventWrite() {
	cacheStat.nWrite++;
}

static inline void eventWriteHit() {
	cacheStat.nWriteHit++;
}

static inline void eventWriteEvict() {
	cacheStat.nWriteEvict++;
}

static inline void printStat() {
	fprintf(stderr, "L1 Cache Stat\n");	
	fprintf(stderr, "read  all / hit / evict = %d / %d / %d\n", cacheStat.nRead, cacheStat.nReadHit, cacheStat.nReadEvict);
	fprintf(stderr, "write all / hit / evict = %d / %d / %d\n", cacheStat.nWrite, cacheStat.nWriteHit, cacheStat.nWriteEvict);
	double hit = (cacheStat.nReadHit + cacheStat.nWriteHit) * 100.0 / (cacheStat.nRead + cacheStat.nWrite);
	fprintf(stderr, "overall hit ratio = %.2f\n", hit);
}

/*
 * TimeTable: simple array of Time with L2_SIZE elements
 *
 */ 

typedef struct _TimeTable {
	Time array[L2_SIZE];
} TimeTable;


TimeTable* TimeTableAlloc() {
	stat.nTimeTableAllocated++;
	stat.nTimeTableActive++;
	if (stat.nTimeTableActive > stat.nTimeTableActiveMax)
		stat.nTimeTableActiveMax++;
	//return (TimeTable*) calloc(sizeof(Time), L2_SIZE);
	return (TimeTable*) MemAlloc();
}

void TimeTableFree(TimeTable* table) {
	stat.nTimeTableActive--;
	stat.nTimeTableFreed++;
	//free(table);
	MemFree(table);
}

static int TimeTableGetIndex(Addr addr) {
    int ret = ((UInt64)addr >> L2_SHIFT) & L2_MASK;
	assert(ret < L2_SIZE);
	return ret;
}

static Time TimeTableGet(TimeTable* table, Addr addr) {
	int index = TimeTableGetIndex(addr);
	return table->array[index];
}

static void TimeTableSet(TimeTable* table, Addr addr, Time time) {
	int index = TimeTableGetIndex(addr);

	MSG(3, "TimeTableSet to addr 0x%llx with index %d\n", &(table->array[index]), index);
	MSG(3, "\t table addr = 0x%llx, array addr = 0x%llx\n", table, &(table->array[0]));
	table->array[index] = time;
}


/*
 * SegTable:
 *
 */ 

typedef struct _SegEntry {
	TimeTable* table;
	TimeTable* vTable;
	Version version;
	int type;
} SegEntry;


typedef struct _Segment {
	SegEntry entry[L1_SIZE];
} SegTable;



SegTable* SegTableAlloc() {
	SegTable* ret = (SegTable*) calloc(sizeof(SegEntry), L1_SIZE);

	int i;
	for (i=0; i<L1_SIZE; i++) {
		ret->entry[i].type = 0;
	}

	stat.nSegTableAllocated++;
	stat.nSegTableActive++;
	if (stat.nSegTableActive > stat.nSegTableActiveMax)
		stat.nSegTableActiveMax++;
	return ret;	
}

void SegTableFree(SegTable* table) {
	int i;
	for (i=0; i<L1_SIZE; i++) {
		if (table->entry[i].table != NULL) {
			TimeTableFree(table->entry[i].table);	
#ifdef USE_VERSION_TABLE
			TimeTableFree(table->entry[i].vTable);	
#endif
		}
	}
	stat.nSegTableActive--;
	free(table);
}

static int SegTableGetIndex(Addr addr) {
	return ((UInt64)addr >> L1_SHIFT) & L1_MASK;
}

#if 0
#ifdef USE_VERSION_TABLE
TimeTable* SegTableGetVersionTableNoVersion(SegTable* segTable, Addr addr) {
	TimeTable* ret = NULL;
	int index = SegTableGetIndex(addr);
	SegEntry* entry = &(segTable->entry[index]);
	MSG(0, "SegTableGetTimeTable: index = %d addr = 0x%llx, version [prev, current] =[%d, %d]\n", 
		index, addr, entry->version, version);

	if (entry->table == NULL || entry->version != version) {
		ret = TimeTableAlloc();
		if (entry->table != NULL) {
			MSG(0, "\tFree Prev TimeTable 0x%llx with version %d\n", entry->table, entry->version);
			TimeTableFree(entry->table);
			TimeTableFree(entry->vTable);
		}
		entry->table = ret;
		entry->vTable = TimeTableAlloc();
	}

	return entry->table;
}

#else
TimeTable* SegTableGetTimeTable(SegTable* segTable, Addr addr, Version version) {
	TimeTable* ret = NULL;
	int index = SegTableGetIndex(addr);
	SegEntry* entry = &(segTable->entry[index]);
	MSG(0, "SegTableGetTimeTable: index = %d addr = 0x%llx, version [prev, current] =[%d, %d]\n", 
		index, addr, entry->version, version);

	if (entry->table == NULL || entry->version != version) {
		ret = TimeTableAlloc();
		if (entry->table != NULL) {
			MSG(0, "\tFree Prev TimeTable 0x%llx with version %d\n", entry->table, entry->version);
			TimeTableFree(entry->table);
		}
		entry->table = ret;
		entry->version = version;
	}

	return entry->table;
}
#endif
#endif

static Bool isType0TimeTable(SegEntry* entry) {
	//return entry->vTable == NULL;
	//return FALSE;
	return entry->type == 0;
	//return TRUE;
}

void SegTableSetTime(SegTable* segTable, Addr addr, Version version, Time time) {
	int index = SegTableGetIndex(addr);
	SegEntry* entry = &(segTable->entry[index]);

	// determine if this is type 0 (no version) or type 1 (with version)
	if (isType0TimeTable(entry)) {
		if (entry->table == NULL) {
			entry->table = TimeTableAlloc();
			entry->version = version;
			TimeTableSet(entry->table, addr, time);	

		} else if (entry->version == version) {
			TimeTableSet(entry->table, addr, time);	

		} else {
#if 0
			TimeTableFree(entry->table);
			entry->table = TimeTableAlloc();
			entry->version = version;
			TimeTableSet(entry->table, addr, time);	
#endif
			// transform into type 1
			entry->type = 1;
			entry->vTable = TimeTableAlloc();
			TimeTableSet(entry->vTable, addr, version);
			TimeTableSet(entry->table, addr, time);
		}

	}  else {
		// type 1 
#if 1
		if (entry->table == NULL) {
			entry->table = TimeTableAlloc();
			entry->vTable = TimeTableAlloc();
		}
#endif

		// set version and time
		assert(entry->type == 1);
		TimeTableSet(entry->vTable, addr, version);
		TimeTableSet(entry->table, addr, time);
	}
}

Time SegTableGetTime(SegTable* segTable, Addr addr, Version version) {
	int index = SegTableGetIndex(addr);
	SegEntry* entry = &(segTable->entry[index]);
	
	// determine if this is type 0 (no version) or type 1 (with version)
	if (isType0TimeTable(entry)) {
		if (entry->table == NULL || entry->version != version) {
			if (entry->table != NULL)
					TimeTableFree(entry->table);
			
			entry->table = TimeTableAlloc();
			entry->version = version;
		}
		return TimeTableGet(entry->table, addr);	
			
	} else {
		if (entry->table == NULL) {
			entry->table = TimeTableAlloc();
			entry->vTable = TimeTableAlloc();
		}

		// set version and time
		Version oldVersion = (Version)TimeTableGet(entry->vTable, addr);
		MSG(0, "\t\t version = [%d, %d]\n", oldVersion, version);
		if (oldVersion != version)
			return 0;
		else
			return TimeTableGet(entry->table, addr);
	}
}

/*
 * ITable - Index Table
 *
 */

typedef struct _ITable {
	SegTable** table;
} ITable;

static int iTableSize = 64;

ITable* ITableAlloc() {
	ITable* ret = malloc(sizeof(ITable));
	ret->table = (SegTable*) calloc(sizeof(SegTable*), iTableSize);
	return ret;
}

void ITableRealloc(ITable* iTable, int newSize) {
	SegTable** oldTable = iTable->table;
	iTable->table = (SegTable*) calloc(sizeof(SegTable*), newSize);
	memcpy(iTable->table, oldTable, sizeof(SegTable*) * iTableSize);

	iTableSize = newSize;
}

void ITableFree(ITable* iTable) {
	assert(iTable != NULL);

	int i;
	for (i=0; i<iTableSize; i++) {
		if (iTable->table[i] != (SegTable*)NULL)
			SegTableFree(iTable->table[i]);
	}
	free(iTable);
}

static SegTable* ITableGetSegTable(ITable* iTable, Index index) {
	assert(iTable != NULL);
	if (index >= iTableSize) {
		ITableRealloc(iTable, iTableSize * 2); 
		assert(0);
	}

	SegTable* ret = iTable->table[index];
	if (ret == NULL) {
		iTable->table[index] = SegTableAlloc();
	}
	return iTable->table[index];
}


/*
 * STable: sparse table that tracks 4GB memory chunks being used
 *
 * Since 64bit address is very sparsely used in a program,
 * we use a sparse table to reduce the memory requirement of the table.
 * Although the walk-through of a table might be pricey,
 * the use of cache will make the frequency of walk-through very low.
 */

typedef struct _SEntry {
	UInt32 	addrHigh;	// upper 32bit in 64bit addr
	ITable* iTable;
} SEntry;

#define STABLE_SIZE		32		// 128GB addr space will be enough

typedef struct _STable {
	SEntry entry[STABLE_SIZE];	
	int writePtr;
} STable;


static STable sTable;

void STableInit() {
	sTable.writePtr = 0;
}

void STableDeinit() {
	int i;

	for (i=0; i<sTable.writePtr; i++) {
		ITableFree(sTable.entry[i].iTable);		
	}
}

ITable* STableGetITable(Addr addr) {
	UInt32 highAddr = (UInt32)((UInt64)addr >> 32);

	// walk-through STable
	int i;
	for (i=0; i<sTable.writePtr; i++) {
		if (sTable.entry[i].addrHigh == highAddr) {
			//MSG(0, "STable Found an existing entry..\n");
			return sTable.entry[i].iTable;	
		}
	}

	// not found - create an entry
	MSG(0, "STable Creating a new Entry..\n");
	stat.nSTableEntry++;

	ITable* ret = ITableAlloc();
	sTable.entry[sTable.writePtr].addrHigh = highAddr;
	sTable.entry[sTable.writePtr].iTable = ret;
	sTable.writePtr++;
	return ret;
}

Time MShadowGetTime(Addr addr, Index index, Version version) {
	ITable* iTable = STableGetITable(addr);
	assert(iTable != NULL);
	SegTable* segTable = ITableGetSegTable(iTable, index);
	assert(segTable != NULL);
	//TimeTable* tTable = SegTableGetTimeTable(segTable, addr, version);
	//assert(tTable != NULL);
	//return TimeTableGet(tTable, addr);	
	Time ret = SegTableGetTime(segTable, addr, version);
	MSG(0, "\tMShadowGetTime at 0x%llx index %d, version %d = %llu\n", 
		addr, index, version, ret);
	return ret;
}

void MShadowSetTime(Addr addr, Index index, Version version, Time time) {
	MSG(0, "MShadowSetTime index %d version %d time %d\n", index, version, time);
	ITable* iTable = STableGetITable(addr);
	assert(iTable != NULL);
	SegTable* segTable = ITableGetSegTable(iTable, index);
	assert(segTable != NULL);
	//TimeTable* tTable = SegTableGetTimeTable(segTable, addr, version);
	//assert(tTable != NULL);
	//TimeTableSet(tTable, addr, time);	
	SegTableSetTime(segTable, addr, version, time);
}


void MShadowSetTimeEvict(Addr addr, Index index, Version version, Time time) {
	MSG(0, "MShadowSetTime index %d version %d time %d\n", index, version, time);
	ITable* iTable = STableGetITable(addr);
	assert(iTable != NULL);
	SegTable* segTable = ITableGetSegTable(iTable, index);
	assert(segTable != NULL);
	SegTableSetTime(segTable, addr, version, time);
}

void MShadowSetFromCache(Addr addr, Index size, Version* vArray, Time* tArray) {
	Index i;
	MSG(0, "MShadowSetFromCache 0x%llx, size %d vArray = 0x%llx tArray = 0x%llx\n", addr, size, vArray, tArray);
	assert(vArray != NULL);
	assert(tArray != NULL);
	for (i=0; i<size; i++)
		MShadowSetTime(addr, i, vArray[i], tArray[i]);
}


#if 0
static Time array[128];
Time* MShadowGet(Addr addr, Index size, Version* vArray) {
	Index i;
	MSG(0, "MShadowGet 0x%llx, size %d\n", addr, size);
	for (i=0; i<size; i++)
		array[i] = MShadowGetTime(addr, i, vArray[i]);
	return array;
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray) {
	MShadowSetFromCache(addr, size, vArray, tArray);
}
#endif

#if 0
Timestamp MShadowGet(Addr addr, Index index, Version version) {	
#if 1
	TEntry* entry = GTableGetTEntry(gTable, addr);
	Timestamp ts = TEntryGet(entry, index, version);
	return ts;
#endif
	//return 0;
}

void MShadowSet(Addr addr, Index index, Version version, Time time) {
#if 1
	TEntry* entry = GTableGetTEntry(gTable, addr);
	TEntryUpdate(entry, index, version, time);
#endif
}
#endif


//#define BYPASS_CACHE
#define CACHE_WITH_VERSION

#define INIT_LEVEL		24	// max index depth
#define WORD_SHIFT 2	// 32bit word

#if 0
#define LINE_SIZE_SHIFT	3
#define LINE_SIZE		(1 << LINE_SIZE_SHIFT)
#define LINE_SIZE_MASK	(LINE_SIZE - 1)
#endif

//#define LINE_SHIFT	10
//#define LINE_NUM	(1 << LINE_SHIFT)
//#define LINE_MASK	(LINE_NUM - 1)

#define STATUS_VALID	1
#define STATUS_DIRTY	2



// forward declarations
void MShadowSetFromCache(Addr addr, Index size, Version* vArray, Time* tArray);
void MShadowSetTimeEvict(Addr addr, Index index, Version version, Time time);


typedef struct _L1Entry {
	UInt64 tag;  
	UInt64 expire;
	UInt32 status;	

} L1Entry;

static L1Entry* tagTable;
static Table* valueTable;
static Table* versionTable;
static int lineNum;
static int lineShift;
static int lineMask;
static int bypassCache = 0;

static inline Bool isValid(L1Entry* entry) {
	return entry->status & STATUS_VALID;
}

static inline void setValid(L1Entry* entry) {
	entry->status |= STATUS_VALID;
}

static inline Bool isDirty(L1Entry* entry) {
	return entry->status & (STATUS_DIRTY | STATUS_VALID);
}

static inline void setDirty(L1Entry* entry) {
	entry->status |= STATUS_DIRTY;
}

static inline void clearDirty(L1Entry* entry) {
	entry->status &= ~STATUS_DIRTY;
}





static inline UInt64 getTag(Addr addr) {
	int nShift = WORD_SHIFT + lineShift;
	UInt64 mask = ~((1 << nShift) - 1);
	return (UInt64)addr & mask;
}

static inline void setTag(L1Entry* entry, Addr addr) {
	entry->tag = getTag(addr);
}

static inline Time* getTimeAddr(int row, int index) {
	return TableGetElementAddr(valueTable, row, index);
}

static inline Version* getVersionAddr(int row, int index) {
	return TableGetElementAddr(versionTable, row, index);
}

static inline Version getVersion(int row, int index) {
	return *TableGetElementAddr(versionTable, row, index);
}


static inline int getLineIndex(Addr addr) {
	int nShift = WORD_SHIFT;
	int ret = (((UInt64)addr) >> nShift) & lineMask;
	assert(ret >= 0 && ret < lineNum);
	return ret;
}

static inline Bool matchVersion(Addr addr, Index index, Version version) {
	int row = getLineIndex(addr);
	return getVersion(row, index) == version;
}

static inline Bool isHit(L1Entry* entry, Addr addr) {
	MSG(0, "isHit addr = 0x%llx, tag = 0x%llx, entry tag = 0x%llx\n",
		addr, getTag(addr), entry->tag);

	return isValid(entry) && (entry->tag == getTag(addr));
}

static inline L1Entry* getEntry(int index) {
	assert(index < lineNum);
	return &(tagTable[index]);
}

static int getFirstOnePosition(int input) {
	int i;

	for (i=0; i<8 * sizeof(int); i++) {
		if (input & (1 << i))
			return i;
	}
	assert(0);
	return 0;
}

void MShadowCacheInit(int cacheSizeMB) {
	int i = 0;
	if (cacheSizeMB == 0) {
		bypassCache = 1;
		fprintf(stderr, "MShadowCacheInit: Bypass Cache\n"); 
		return;
	}
	lineNum = cacheSizeMB * 1024 * 256;
	lineShift = getFirstOnePosition(lineNum);
	lineMask = lineNum - 1;
	fprintf(stderr, "MShadowCacheInit: total size: %d MB, lineNum %d, lineShift %d, lineMask 0x%x\n", 
		cacheSizeMB, lineNum, lineShift, lineMask);

	tagTable = malloc(sizeof(L1Entry) * lineNum);
	for (i = 0; i < lineNum; i++) {
		tagTable[i].status = 0x0;
		tagTable[i].tag = 0x0;
	}
	valueTable = TableCreate(lineNum, INIT_LEVEL);
	versionTable = TableCreate(lineNum, INIT_LEVEL);

	MSG(0, "MShadowCacheInit: value Table created row %d col %d at addr 0x%x\n", 
		lineNum, INIT_LEVEL, valueTable->array);
}

void MShadowCacheDeinit() {
	if (bypassCache == 1)
		return;

	printStat();
	free(tagTable);
	TableFree(valueTable);
	TableFree(versionTable);
}

L1Entry* MShadowCacheEvict(Addr addr, int row, int size, Version* vArray) {
	MSG(0, "MShadowCacheEvict 0x%llx, row=%d size=%d vArray = 0x%llx\n", 
		addr, row, size, vArray);

	L1Entry* entry = getEntry(row);
	assert(isDirty(entry));

	// copy timestamps to the target TimeTable
	// if 2nd cache is used, different scheme required		
	int i;
	Time* tArray = getTimeAddr(row, 0);
	//MShadowSetFromCache(addr, size, vArray, tArray);
	for (i=size-1; i>=0; i--) {
		if (matchVersion(addr, i, vArray[i])) {
			// write to MShadow
			MShadowSetTimeEvict(addr, i, vArray[i], tArray[i]);
		}
	}
	
	return entry;
}

void MShadowFetchLine(L1Entry* entry, Addr addr, Index size, Version* vArray) {
	int index;
	int row = getLineIndex(addr);

	// copy version
	Version* versionAddr = (Version*) getVersionAddr(row, 0);
	memcpy(versionAddr, vArray, sizeof(Version) * size);

	// bring MShadow data
	Time* destAddr = getTimeAddr(row, 0);
	for (index=0; index<size; index++) {
		*(destAddr + index) = MShadowGetTime(addr, index, vArray[index]);
	}

	entry->tag = getTag(addr);
	setValid(entry);
	clearDirty(entry);
}



static Time tempArray[1000];
Time* MShadowGetNoCache(Addr addr, Index size, Version* vArray) {
	Index i;
	for (i=0; i<size; i++)
		tempArray[i] = MShadowGetTime(addr, i, vArray[i]);
	return tempArray;	
}

void MShadowSetNoCache(Addr addr, Index size, Version* vArray, Time* tArray) {
	MShadowSetFromCache(addr, size, vArray, tArray);
}


Time* MShadowGet(Addr addr, Index size, Version* vArray) {
	int row = getLineIndex(addr);
	int i;

	if (bypassCache == 1) {
		return MShadowGetNoCache(addr, size, vArray);
	}


	MSG(0, "MShadowGet 0x%llx, size %d vArray = 0x%llx \n", addr, size, vArray);
	eventRead();

	L1Entry* entry = getEntry(row);
	Time* destAddr = getTimeAddr(row, 0);
	if (isHit(entry, addr)) {
		eventReadHit();
		MSG(0, "\t cache hit at 0x%llx \n", destAddr);
		MSG(0, "\t value0 %d value1 %d \n", destAddr[0], destAddr[1]);
		
	} else {
		// Unfortunately, this access results in a miss
		// 1. evict a line	
		if (isDirty(entry)) {
			MSG(0, "\t eviction required \n", destAddr);
			eventReadEvict();
			MShadowCacheEvict(addr, row, size, vArray);
		}

		// 2. read line from MShadow to the evicted line
		MSG(0, "\t write values to cache \n", destAddr);
		MShadowFetchLine(entry, addr, size, vArray);
	}

	// check versions and if outdated, set to 0
	MSG(0, "\t checking versions \n", destAddr);
	Version* vAddr = (Version*) getVersionAddr(row, 0);

	for (i=size-1; i>=0; i--) {
		if (vAddr[i] ==  vArray[i]) {
			// no need to check next iterations
			break;

		} else {
			// update version number 	
			// and set timestamp to zero
			vAddr[i] = vArray[i];
			destAddr[i] = 0ULL;
		}
	}

	return destAddr;
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray) {
	MSG(0, "MShadowSet 0x%llx, size %d vArray = 0x%llx tArray = 0x%llx\n", addr, size, vArray, tArray);
	if (bypassCache == 1) {
		MShadowSetNoCache(addr, size, vArray, tArray);
		return;
	}

	int row = getLineIndex(addr);
	eventWrite();

	L1Entry* entry = getEntry(row);

	assert(row < lineNum);

	if (isHit(entry, addr)) {
		eventWriteHit();

	} else {
		if (isDirty(entry)) {
			MSG(0, "\t eviction required\n", row, 0);
			eventWriteEvict();
			MShadowCacheEvict(addr, row, size, vArray);
		}
	} 		

	// copy Timestamps
	Time* destAddr = getTimeAddr(row, 0);
	memcpy(destAddr, tArray, sizeof(Time) * size);

	// copy Versions
	Version* versionAddr = (Version*) getVersionAddr(row, 0);
	int i;
	for (i=size-1; i>=0; i--) {
		if (versionAddr[i] == vArray[i])
			break;
		else
			versionAddr[i] = vArray[i];
	}
	//memcpy(versionAddr, vArray, sizeof(Version) * size);

	setDirty(entry);
}


/*
 * Init / Deinit
 */
UInt MShadowInit(int cacheSizeMB) {
	STableInit();
	MemAllocInit(sizeof(TimeTable));
	MShadowCacheInit(cacheSizeMB);
}


UInt MShadowDeinit() {
	printMemStat();
	STableDeinit();
	MemAllocDeinit();
	MShadowCacheDeinit();
}

#endif

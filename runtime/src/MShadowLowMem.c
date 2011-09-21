#include "defs.h"

//#define DUMMY_SHADOW
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


// SEGTABLE Parameters
#define SEGTABLE_MASK 	0xfffff
#define SEGTABLE_SHIFT	12
//#define SEGTABLE_MASK 	0xffff
//#define SEGTABLE_SHIFT	16
#define SEGTABLE_SIZE 	(SEGTABLE_MASK + 1)

// TimeTable Parameters
#define TIMETABLE_MASK 0x3ff
//#define TIMETABLE_MASK 0x3fff
#define TIMETABLE_SIZE (TIMETABLE_MASK + 1)
#define WORD_SHIFT 2

// Cache Constatns and Parameters
#define CACHE_WITH_VERSION
#define INIT_LEVEL		24	// max index depth
#define STATUS_VALID	1
#define STATUS_DIRTY	2
#define STATUS_32BIT	3

// Timetable Type
#define TYPE_NOVERSION	0
#define TYPE_VERSION	1
#define TYPE_HYBRID		2
//#define TIMETABLE_TYPE	TYPE_VERSION

#define TYPE_64BIT	0
#define TYPE_32BIT	1

static int timetableType;

typedef struct _TimeTable {
	int type;
	Time* array;
} TimeTable;

typedef struct _SegEntry {
	TimeTable* table;
	Version version;
	//TimeTable* vTable;
	//int type;
	//int counter;
} SegEntry;


typedef struct _Segment {
	SegEntry entry[SEGTABLE_SIZE];
	int level;
} SegTable;


/*
 * MemStat
 */
typedef struct _MemStat {
	UInt64 nSTableEntry;

	UInt64 nSegTableAllocated;
	UInt64 nSegTableActive;
	UInt64 nSegTableActiveMax;

	UInt64 nTimeTableAllocated[2];
	UInt64 nVersionTableAllocated;
	UInt64 nTimeTableFreed[2];
	UInt64 nTimeTableActive;
	UInt64 nTimeTableActiveMax;
	UInt64 nSegTableNewAlloc[100];
	UInt64 nTimeTableNewAlloc[100];
	UInt64 nTimeTableConvert[100];
	UInt64 nTimeTableRealloc[100];
	UInt64 nTimeTableReallocTotal;
	UInt64 nEvict[100];
	UInt64 nEvictTotal;
	UInt64 nCacheEvictLevelTotal;
	UInt64 nCacheEvictLevelEffective;
	UInt64 nCacheEvict;

} MemStat;

static MemStat stat;

/*
 * CacheStat
 */

typedef struct _L1Stat {
	UInt64 nRead;
	UInt64 nReadHit;
	UInt64 nReadEvict;
	UInt64 nWrite;
	UInt64 nWriteHit;
	UInt64 nWriteEvict;
} L1Stat;

L1Stat cacheStat;


double getSizeMB(UInt64 nUnit, UInt64 size) {
	return (nUnit * size) / (1024.0 * 1024.0);	
}

/*
 * Cache size when a specific # of levels are used
 */
static int cacheMB;
double getCacheSize(int level) {
	return (double)(cacheMB * 4.0 + level * cacheMB * 2);
}

static void printCacheStat() {
	fprintf(stderr, "\nShadow Memory Cache Stat\n");	
	fprintf(stderr, "\tread  all / hit / evict = %lld / %lld / %lld\n", 
		cacheStat.nRead, cacheStat.nReadHit, cacheStat.nReadEvict);
	fprintf(stderr, "\twrite all / hit / evict = %lld / %lld / %lld\n", 
		cacheStat.nWrite, cacheStat.nWriteHit, cacheStat.nWriteEvict);
	double hitRead = cacheStat.nReadHit * 100.0 / cacheStat.nRead;
	double hitWrite = cacheStat.nWriteHit * 100.0 / cacheStat.nWrite;
	double hit = (cacheStat.nReadHit + cacheStat.nWriteHit) * 100.0 / (cacheStat.nRead + cacheStat.nWrite);
	fprintf(stderr, "\tCache hit (read / write / overall) = %.2f / %.2f / %.2f\n", 
		hitRead, hitWrite, hit);
	fprintf(stderr, "\tEvict (total / levelAvg / levelEffective) = %d / %.2f / %.2f\n\n", 
		stat.nCacheEvict, 
		(double)stat.nCacheEvictLevelTotal / stat.nCacheEvict, 
		(double)stat.nCacheEvictLevelEffective / stat.nCacheEvict);
}


static void printMemReqStat() {
	//fprintf(stderr, "Overall allocated = %d, converted = %d, realloc = %d\n", 
	//	totalAlloc, totalConvert, totalRealloc);
	double segSize = getSizeMB(stat.nSegTableActiveMax, sizeof(SegTable));
	UInt64 nTable0 = stat.nTimeTableAllocated[0] - stat.nTimeTableFreed[0];
	int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TIMETABLE_SIZE / 2);
	UInt64 nTable1 = stat.nTimeTableAllocated[1] - stat.nTimeTableFreed[1];
	int sizeTable32 = sizeof(TimeTable) + sizeof(Time) * TIMETABLE_SIZE;
	
	double tTableSize0 = getSizeMB(nTable0, sizeTable64);
	double tTableSize1 = getSizeMB(nTable1, sizeTable32);
	double tTableSize = tTableSize0 + tTableSize1;
	double vTableSize = getSizeMB(stat.nVersionTableAllocated, sizeof(TimeTable));
	double cacheSize = getCacheSize(getMaxActiveLevel());
	double totalSize = segSize + tTableSize + vTableSize + cacheSize;
	//minTotal += getCacheSize(2);
	fprintf(stderr, "\nRequired Memory Analysis\n");
	fprintf(stderr, "\tShadowMemory (SegTable / TTable / VTable) = %.2f / %.2f / %.2f\n",
		segSize, tTableSize, vTableSize);
	fprintf(stderr, "\tReqMemSize (Total / Cache / Shadow) = %.2f / %.2f / %.2f \n",
		totalSize, cacheSize, segSize + tTableSize + vTableSize);  
}

static void printLevelStat() {
	int i;
	int totalAlloc = 0;
	int totalConvert = 0;
	int totalRealloc = 0;
	double minTotal = 0;

	fprintf(stderr, "\nLevel Specific Statistics\n");

	for (i=0; i<=getMaxActiveLevel(); i++) {
		totalAlloc += stat.nTimeTableNewAlloc[i];
		totalConvert += stat.nTimeTableConvert[i];
		totalRealloc += stat.nTimeTableRealloc[i];

		double sizeSegTable = getSizeMB(stat.nSegTableNewAlloc[i], sizeof(SegTable));
		double sizeTimeTable = getSizeMB(stat.nTimeTableNewAlloc[i], sizeof(TimeTable));
		double sizeVersionTable = getSizeMB(stat.nTimeTableConvert[i], sizeof(TimeTable));
		double sizeLevel = sizeSegTable + sizeTimeTable + sizeVersionTable;
		double reallocPercent = (double)stat.nTimeTableRealloc[i] * 100.0 / (double)stat.nEvict[i];

		if (i < 2)
			minTotal += sizeLevel;
		
		fprintf(stderr, "\tLevel [%2d] SegTable=%.2f, TTable=%.2f, VTable=%.2f Sum=%.2f MB\n", 
			i, sizeSegTable, sizeTimeTable, sizeVersionTable, sizeLevel, sizeLevel);
		fprintf(stderr, "\t\tReallocPercent=%.2f, Evict=%lld, Realloc=%lld\n",
			reallocPercent, stat.nEvict[i], stat.nTimeTableRealloc[i]);
			
		//fprintf(stderr, "Level [%2d] allocated = %d, converted = %d, realloc = %d evict = %d\n", 
		//	i, stat.nTimeTableNewAlloc[i], stat.nTimeTableConvert[i], 
		//	stat.nTimeTableRealloc[i], stat.nEvict[i]);
	}
}

static void printMemStatAllocation() {
	fprintf(stderr, "\nShadow Memory Allocation Stats\n");
	fprintf(stderr, "\tnSTableEntry = %d\n", stat.nSTableEntry);
	fprintf(stderr, "\tnSegTable: Alloc / Active / ActiveMax = %lld / %lld / %lld\n",
		 stat.nSegTableAllocated, stat.nSegTableActive, stat.nSegTableActiveMax);
	fprintf(stderr, "\tnTimeTable(type %d): Alloc / Freed / ActiveMax= %lld, %lld / %lld, %lld / %lld\n",
		 timetableType, stat.nTimeTableAllocated[0], stat.nTimeTableAllocated[1],
		 stat.nTimeTableFreed[0], stat.nTimeTableFreed[1], stat.nTimeTableActiveMax);
	fprintf(stderr, "\tnVersionTable: Alloc = %lld\n", stat.nVersionTableAllocated);
	fprintf(stderr, "\tnTotal Evict: %lld, Realloc: %lld\n", 
		stat.nEvictTotal, stat.nTimeTableReallocTotal);
	fprintf(stderr, "\tRealloc / Evict Percentage: %.2f %%\n", 
		(double)stat.nTimeTableReallocTotal * 100.0 / stat.nEvictTotal);
	UInt64 nAllocated = stat.nTimeTableAllocated[0] + stat.nTimeTableAllocated[1];
	fprintf(stderr, "\tNewAlloc / Evict Percentage: %.2f %%\n", 
		(double)nAllocated * 100.0 / stat.nEvictTotal);
}

static void printMemStat() {
	printMemStatAllocation();
	printLevelStat();
	printCacheStat();
	printMemReqStat();
}

static inline void eventCacheEvict(int total, int effective) {
	stat.nCacheEvictLevelTotal += total;
	stat.nCacheEvictLevelEffective += effective;
	stat.nCacheEvict++;
}

static inline void eventTimeTableNewAlloc(int level) {
	stat.nTimeTableNewAlloc[level]++;
}

static inline void eventTimeTableConvert(int level) {
	stat.nTimeTableConvert[level]++;
}

static inline void eventTimeTableRealloc(int level) {
	stat.nTimeTableRealloc[level]++;
	stat.nTimeTableReallocTotal++;
}

static inline void eventEvict(int level) {
	stat.nEvict[level]++;
	stat.nEvictTotal++;
}
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

/*
 * TimeTable: simple array of Time with TIMETABLE_SIZE elements
 *
 */ 


TimeTable* TimeTableAlloc(int type) {
	stat.nTimeTableAllocated[type]++;
	stat.nTimeTableActive++;
	if (stat.nTimeTableActive > stat.nTimeTableActiveMax)
		stat.nTimeTableActiveMax++;
	//return (TimeTable*) calloc(sizeof(Time), TIMETABLE_SIZE);
	TimeTable* ret = malloc(sizeof(TimeTable));
	ret->type = type;
	int size = TIMETABLE_SIZE;
	if (type == TYPE_64BIT)
		size >>= 1;
	ret->array = calloc(sizeof(Time), size);
	return ret;
	
	//return (TimeTable*) MemAlloc();
	//TimeTable* ret = malloc(sizeof(TimeTable));
	//bzero(ret, sizeof(TimeTable));
	//return ret;
}


void TimeTableFree(TimeTable* table) {
	stat.nTimeTableActive--;
	stat.nTimeTableFreed[table->type]++;
	//free(table);
	//MemFree(table);
	free(table->array);
	free(table);
}

TimeTable* VersionTableAlloc() {
	stat.nVersionTableAllocated++;
	//return (TimeTable*) calloc(sizeof(Time), TIMETABLE_SIZE);
	return (TimeTable*) MemAlloc();
}

void VersionTableFree(TimeTable* table) {
	TimeTableFree(table);
}

static inline int TimeTableGetIndex(Addr addr, int type) {
    int ret = ((UInt64)addr >> WORD_SHIFT) & TIMETABLE_MASK;
	assert(ret < TIMETABLE_SIZE);
	if (type == TYPE_64BIT)
		ret >>= 1;
	return ret;
}

// note that TimeTableGet is not affected by read width
static inline Time TimeTableGet(TimeTable* table, Addr addr) {
	int index = TimeTableGetIndex(addr, table->type);
	return table->array[index];
}

static inline void TimeTableSet(TimeTable* table, Addr addr, Time time) {
	int index = TimeTableGetIndex(addr, table->type);
	MSG(3, "TimeTableSet to addr 0x%llx with index %d\n", &(table->array[index]), index);
	MSG(3, "\t table addr = 0x%llx, array addr = 0x%llx\n", table, &(table->array[0]));
	table->array[index] = time;
}


/*
 * SegTable:
 *
 */ 

#if 0
static inline int getTimeTableType(SegEntry* entry) {
	return entry->type;
}
#endif

SegTable* SegTableAlloc(int level) {
	SegTable* ret = (SegTable*) calloc(sizeof(SegEntry), SEGTABLE_SIZE);
	ret->level = level;

	int i;
#if 0
	for (i=0; i<SEGTABLE_SIZE; i++) {
		if (timetableType == TYPE_VERSION)
			ret->entry[i].type = TYPE_VERSION;
		else
			ret->entry[i].type = TYPE_NOVERSION;
	}
#endif

	stat.nSegTableAllocated++;
	stat.nSegTableNewAlloc[level]++;
	stat.nSegTableActive++;
	if (stat.nSegTableActive > stat.nSegTableActiveMax)
		stat.nSegTableActiveMax++;
	return ret;	
}

void SegTableFree(SegTable* table) {
	int i;
	for (i=0; i<SEGTABLE_SIZE; i++) {
		if (table->entry[i].table != NULL) {
			TimeTableFree(table->entry[i].table);	
			//if (getTimeTableType(&(table->entry[i])) == TYPE_VERSION)
			//	TimeTableFree(table->entry[i].vTable);	
		}
	}
	stat.nSegTableActive--;
	free(table);
}

static inline int SegTableGetIndex(Addr addr) {
	return ((UInt64)addr >> SEGTABLE_SHIFT) & SEGTABLE_MASK;
}

// convert 64 bit format into 32 bit format
TimeTable* convertTimeTable(TimeTable* table) {
	assert(table->type == TYPE_64BIT);
	TimeTable* ret = TimeTableAlloc(TYPE_32BIT);
	int i;
	for (i=0; i<TIMETABLE_SIZE/2; i++) {
		ret->array[i*2] = ret->array[i];
		ret->array[i*2 + 1] = ret->array[i];
	}
	
	return ret;
	
}

void SegEntrySetTimeAllocate(SegEntry* entry, Addr addr, Time time, int level, int type) {
	assert(entry != NULL);
	if (entry->table == NULL) {
		eventTimeTableNewAlloc(level);

	} else {
		eventTimeTableRealloc(level);
		TimeTableFree(entry->table);
	}
	entry->table = TimeTableAlloc(type);
	TimeTableSet(entry->table, addr, time);	
}

void SegEntrySetTimeNoAllocate(SegEntry* entry, Addr addr, Time time, int level, int type) {
	assert(entry != NULL);
	if (entry->table == NULL) {
		eventTimeTableNewAlloc(level);
		entry->table = TimeTableAlloc(type);

	} else if (entry->table->type == TYPE_64BIT && type == TYPE_32BIT) {
		TimeTable* converted = convertTimeTable(entry->table);
		TimeTableFree(entry->table);
		entry->table = converted;
	}
	TimeTableSet(entry->table, addr, time);	
}

Time SegEntryGetTime(SegEntry* entry, Addr addr) {
	assert(entry != NULL);

	if (entry->table == NULL) {
		return 0ULL;
	} else {
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
		iTable->table[index] = SegTableAlloc(index);
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
	//MSG(0, "STable Creating a new Entry..\n");
	stat.nSTableEntry++;

	ITable* ret = ITableAlloc();
	sTable.entry[sTable.writePtr].addrHigh = highAddr;
	sTable.entry[sTable.writePtr].iTable = ret;
	sTable.writePtr++;
	return ret;
}

SEntry* STableGetSEntry(Addr addr) {
	UInt32 highAddr = (UInt32)((UInt64)addr >> 32);

	// walk-through STable
	int i;
	for (i=0; i<sTable.writePtr; i++) {
		if (sTable.entry[i].addrHigh == highAddr) {
			//MSG(0, "STable Found an existing entry..\n");
			return &sTable.entry[i];	
		}
	}

	// not found - create an entry
	MSG(0, "STable Creating a new Entry..\n");
	stat.nSTableEntry++;

	SEntry* ret = &sTable.entry[sTable.writePtr];
	ITable* iTable = ITableAlloc();
	ret->addrHigh = highAddr;
	ret->iTable = iTable;
	sTable.writePtr++;
	return ret;

}


typedef struct _L1Entry {
	UInt64 tag;  
	Version version;
	int lastSize;
	UInt32 status;	

} L1Entry;

static L1Entry* tagTable;
static Table* valueTable;
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

static inline Time* getTimeAddr(int row, int index) {
	return TableGetElementAddr(valueTable, row, index);
}


static int getLineIndex(Addr addr) {
	int nShift = WORD_SHIFT;
	int ret = (((UInt64)addr) >> nShift) & lineMask;
	assert(ret >= 0 && ret < lineNum);
	return ret;
}

static inline Bool isHit(L1Entry* entry, Addr addr) {
	MSG(3, "isHit addr = 0x%llx, tag = 0x%llx, entry tag = 0x%llx\n",
		addr, entry->tag, entry->tag);

	return isValid(entry) && (entry->tag == addr);
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

	MSG(3, "MShadowCacheInit: value Table created row %d col %d at addr 0x%x\n", 
		lineNum, INIT_LEVEL, valueTable->array);
}

void MShadowCacheDeinit() {
	if (bypassCache == 1)
		return;

	//printStat();
	free(tagTable);
	TableFree(valueTable);
}

static inline Bool isValidVersion(Version prev, Version current) {
	if (current <= prev)
		return TRUE;
	else
		return FALSE;
}

#define MIN(a, b) ((a) < (b))? a : b

static inline int getAccessWidthType(L1Entry* entry) {
	return TYPE_64BIT;
}

void MShadowEvict(L1Entry* cacheEntry, Addr addr, int size, Version* vArray) {
	MSG(0, "\tMShadowEvict 0x%llx, size=%d \n", 
		addr, size);

	if (!isDirty(cacheEntry)) {
		return;		
	}

	int row = getLineIndex(addr);
	Time* tArray = getTimeAddr(row, 0);

	SEntry* sEntry = STableGetSEntry(addr);
	ITable* iTable = sEntry->iTable;
	assert(iTable != NULL);

	int segIndex = SegTableGetIndex(addr);
	int index;
	int startInvalid = size - 1;
	int debug = 0;
	int type = getAccessWidthType(cacheEntry);

	for (index=0; index<size; index++) {
		if (isValidVersion(cacheEntry->version, vArray[index])) {
			// evict if the version in cache line is valid
			SegTable* segTable = ITableGetSegTable(iTable, index);
			eventEvict(segTable->level);
			SegEntry* segEntry = &(segTable->entry[segIndex]);
			Version mshadowVersion = segEntry->version;

			// reallocation required?
			if (isValidVersion(mshadowVersion, vArray[index])) {
				SegEntrySetTimeNoAllocate(segEntry, addr, tArray[index], segTable->level, type); 

			} else {
				// reallocate table
				SegEntrySetTimeAllocate(segEntry, addr, tArray[index], segTable->level, type); 
			}
			segEntry->version = vArray[size-1];
				
		} else {
			// once the version number is out-of-date,
			// no need to check upper levels
			startInvalid = index;
			break;
		}
	}

	if (debug) 
		fprintf(stderr, "\n");
	eventCacheEvict(size, index);
	return cacheEntry;
}

void MShadowFetch(L1Entry* entry, Addr addr, Index size, Version* vArray, Time* destAddr, int type) {
	MSG(0, "\tMShadowFetch 0x%llx, size %d \n", addr, size);

	SEntry* sEntry = STableGetSEntry(addr);
	ITable* iTable = sEntry->iTable;
	assert(iTable != NULL);
	int segIndex = SegTableGetIndex(addr);

	clearDirty(entry);
	int index;
	int startInvalid = -1;
	for (index=0; index<size; index++) {
		SegTable* segTable = ITableGetSegTable(iTable, index);
		SegEntry* segEntry = &(segTable->entry[segIndex]);
		Version version = segEntry->version;

		if (isValidVersion(version, vArray[index])) {
			Time time = SegEntryGetTime(segEntry, addr);
			destAddr[index] = time;

		} else {
			if (startInvalid == -1)
				startInvalid = index;
			destAddr[index] = 0ULL;
			setDirty(entry);
			//break;
		}
	}

	entry->tag = addr;
	entry->version = vArray[size-1];
	entry->lastSize = size;
	setValid(entry);
}



static Time tempArray[1000];
Time* MShadowGetNoCache(Addr addr, Index size, Version* vArray, int type) {
	SEntry* sEntry = STableGetSEntry(addr);
	ITable* iTable = sEntry->iTable;
	int segIndex = SegTableGetIndex(addr);

	Index i;
	for (i=0; i<size; i++) {
		SegTable* segTable = ITableGetSegTable(iTable, i);
		SegEntry* segEntry = &(segTable->entry[segIndex]);
		Version version = segEntry->version;

		if (version < vArray[i]) {
			Time time = SegEntryGetTime(segEntry, addr);
			tempArray[i] = time;
		} else {
			tempArray[i] = 0ULL;
		}
	}

	return tempArray;	
}

void MShadowSetNoCache(Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	SEntry* sEntry = STableGetSEntry(addr);
	ITable* iTable = sEntry->iTable;
	int segIndex = SegTableGetIndex(addr);
	Index i;
	for (i=0; i<size; i++) {
		SegTable* segTable = ITableGetSegTable(iTable, i);
		SegEntry* segEntry = &(segTable->entry[segIndex]);
		Version version = segEntry->version;
		// version is up to date, write to MShadow
		// reallocation required?
		if (version < vArray[i]) {
			// reallocate table
			SegEntrySetTimeAllocate(segEntry, addr, tArray[i], segTable->level, type); 
		} else {
			SegEntrySetTimeNoAllocate(segEntry, addr, tArray[i], segTable->level, type); 
		}
		segEntry->version = vArray[size-1];
	}
}

#ifdef DUMMY_SHADOW

Time* MShadowGet(Addr addr, Index size, Version* vArray, UInt32 width) {
	return tempArray;
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
}
#else

Time* MShadowGet(Addr addr, Index size, Version* vArray, UInt32 width) {
	MSG(0, "MShadowGet 0x%llx, size %d \n", addr, size);
	int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	if (bypassCache == 1) {
		return MShadowGetNoCache(addr, size, vArray, type);
	}

	eventRead();
	int row = getLineIndex(addr);
	Time* destAddr = getTimeAddr(row, 0);
	L1Entry* entry = getEntry(row);

	if (isHit(entry, addr)) {
		eventReadHit();
		MSG(3, "\t cache hit at 0x%llx \n", destAddr);
		MSG(3, "\t value0 %d value1 %d \n", destAddr[0], destAddr[1]);
		
	} else {
		// Unfortunately, this access results in a miss
		// 1. evict a line	
		MSG(3, "\t eviction required \n", destAddr);
		eventReadEvict();
		//Addr evictAddr = getAddrFromTag(entry->tag, row);
		MShadowEvict(entry, entry->tag, entry->lastSize, vArray);

		// 2. read line from MShadow to the evicted line
		MSG(3, "\t write values to cache \n", destAddr);
		MShadowFetch(entry, addr, size, vArray, destAddr, type);
		return destAddr;
	}

	// check versions and if outdated, set to 0
	MSG(3, "\t checking versions \n", destAddr);
	Version version = entry->version;

	int i;
	for (i=size-1; i>=0; i--) {
		if (entry->lastSize <= i || 
			!isValidVersion(version, vArray[i])) {
			destAddr[i] = 0ULL;
			setDirty(entry);

		} else {
			// if version is larger, the level is still valid
			break;

		} 	
	}
	entry->version = vArray[size-1];
	entry->lastSize = size;
	return destAddr;
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	MSG(0, "MShadowSet 0x%llx, size %d \n",
		addr, size);
	
	int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	eventWrite();
	if (bypassCache == 1) {
		MShadowSetNoCache(addr, size, vArray, tArray, type);
		return;
	}

	int row = getLineIndex(addr);
	L1Entry* entry = getEntry(row);
	assert(row < lineNum);

	if (isHit(entry, addr)) {
		eventWriteHit();

	} else {
		eventWriteEvict();
		MShadowEvict(entry, entry->tag, entry->lastSize, vArray);
	} 		

	// copy Timestamps
	Time* destAddr = getTimeAddr(row, 0);
	memcpy(destAddr, tArray, sizeof(Time) * size);
	setValid(entry);
	setDirty(entry);
	//entry->tag = getTag(addr);
	entry->tag = addr;
	entry->version = vArray[size-1];
	entry->lastSize = size;
}
#endif


/*
 * Init / Deinit
 */
UInt MShadowInit(int cacheSizeMB, int type) {
	fprintf("[kremlin] MShadow Init with cache % MB, TimeTableType = %d, TimeTableSize = %d\n",
		cacheSizeMB, type, sizeof(TimeTable));
	timetableType = type;
	cacheMB = cacheSizeMB;
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

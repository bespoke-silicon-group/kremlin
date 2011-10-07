#include "defs.h"

//#define DUMMY_SHADOW
#if TYPE_MSHADOW == MSHADOW_CACHE

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "debug.h"
#include "CRegion.h"
#include "MShadow.h"
#include "Table.h"

#define TIME_TABLE_BTV		0
#define TIME_TABLE_VERSION	1	
#define TIME_TABLE_ADAPTIVE	2

#define TIME_TABLE_TYPE		TIME_TABLE_VERSION

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

//#define CACHE_VERSION_SHIFT	0

static int timetableType;

typedef struct _TimeTable {
	int type;
	int useVersion;
	Time* array;
	Version* version;
} TimeTable;



#define MAX_LEVEL	32

typedef struct _SegEntry {
	UInt8	noBTV[MAX_LEVEL];
	UInt64	nAccess[MAX_LEVEL];
	Version vArray[MAX_LEVEL];
	TimeTable* tArray[MAX_LEVEL];
} SegEntry;


typedef struct _Segment {
	SegEntry* entry[SEGTABLE_SIZE];
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
	UInt64 nTimeTableFreed[2];
	UInt64 nVersionTableAllocated[2];
	UInt64 nVersionTableFreed[2];
	UInt64 nTimeTableConvert32;
	UInt64 nTimeTableActive;
	UInt64 nTimeTableActiveMax;
	UInt64 nSegTableNewAlloc[100];
	UInt64 nSegEntryAlloc;
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
	double segEntrySize = getSizeMB(stat.nSegEntryAlloc, sizeof(SegEntry));

	UInt64 nTable0 = stat.nTimeTableAllocated[0] - stat.nTimeTableFreed[0];
	int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TIMETABLE_SIZE / 2);
	UInt64 nTable1 = stat.nTimeTableAllocated[1] - stat.nTimeTableFreed[1];
	int sizeTable32 = sizeof(TimeTable) + sizeof(Time) * TIMETABLE_SIZE;
	double tTableSize0 = getSizeMB(nTable0, sizeTable64);
	double tTableSize1 = getSizeMB(nTable1, sizeTable32);
	double tTableSize = tTableSize0 + tTableSize1;


	int sizeVersion64 = sizeof(Version) * (TIMETABLE_SIZE / 2);
	int sizeVersion32 = sizeof(Version) * (TIMETABLE_SIZE);
	UInt64 nVTable0 = stat.nVersionTableAllocated[0] - stat.nVersionTableFreed[0];
	UInt64 nVTable1 = stat.nVersionTableAllocated[1] - stat.nVersionTableFreed[1];
	double vTableSize0 = getSizeMB(nVTable0, sizeVersion64);
	double vTableSize1 = getSizeMB(nVTable1, sizeVersion32);
	double vTableSize = vTableSize0 + vTableSize1;


	double cacheSize = getCacheSize(getMaxActiveLevel());
	double totalSize = segSize + tTableSize + vTableSize + cacheSize;
	//minTotal += getCacheSize(2);
	fprintf(stderr, "\nRequired Memory Analysis\n");
	fprintf(stderr, "\tShadowMemory (SegTable / TTable / VTable) = %.2f / %.2f / %.2f\n",
		segSize + segEntrySize, tTableSize, vTableSize);
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
	fprintf(stderr, "\tbTimeTable Convert: %lld\n", stat.nTimeTableConvert32);
	fprintf(stderr, "\tnVersionTable: Alloc = %lld / %lld\n", 
		stat.nVersionTableAllocated[0], stat.nVersionTableAllocated[1]);
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

static inline void eventSegEntryAlloc() {
	stat.nSegEntryAlloc++;
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

static inline void eventTimeTableConvertTo32() {
	stat.nTimeTableConvert32++;
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

static int TimeTableEntrySize(int type) {
	int size = TIMETABLE_SIZE;
	if (type == TYPE_64BIT)
		size >>= 1;
	return size;
}

/*
 * TimeTable: simple array of Time with TIMETABLE_SIZE elements
 *
 */ 

static inline TimeTable* TimeTableAlloc(int sizeType, int useVersion) {
	stat.nTimeTableAllocated[sizeType]++;
	stat.nTimeTableActive++;
	if (stat.nTimeTableActive > stat.nTimeTableActiveMax)
		stat.nTimeTableActiveMax++;

	assert(sizeType == TYPE_32BIT || sizeType == TYPE_64BIT);
	int size = TimeTableEntrySize(sizeType);
	TimeTable* ret = malloc(sizeof(TimeTable));
	ret->array = calloc(sizeof(Time), size);
	ret->type = sizeType;
	ret->useVersion = useVersion;

	if (useVersion == 1) {
		ret->version = calloc(sizeof(Version), size);
		stat.nVersionTableAllocated[sizeType]++;
	}
	return ret;
}


static inline void TimeTableFree(TimeTable* table) {
	stat.nTimeTableActive--;
	int sizeType = table->type;
	assert(sizeType == TYPE_32BIT || sizeType == TYPE_64BIT);
	stat.nTimeTableFreed[table->type]++;
	free(table->array);
	if (table->useVersion) {
		assert(table->version != NULL);
		free(table->version);
		stat.nVersionTableFreed[table->type]++;
	}
	free(table);
}


static inline int TimeTableGetIndex(Addr addr, int type) {
    int ret = ((UInt64)addr >> WORD_SHIFT) & TIMETABLE_MASK;
	assert(ret < TIMETABLE_SIZE);
	if (type == TYPE_64BIT)
		ret >>= 1;
	return ret;
}

// note that TimeTableGet is not affected by access width
static inline Time TimeTableGet(TimeTable* table, Addr addr, Version ver) {
	int index = TimeTableGetIndex(addr, table->type);
	Time ret = 0ULL;
	if (table->useVersion == 0)
		ret = table->array[index];

	else if (table->version[index] >= ver) {
		ret = table->array[index];

	} else {
		table->version[index] = ver;
		table->array[index] = 0ULL;
		ret = 0ULL;
	}
	MSG(0, "\t\t\t table = 0x%x, index = %d, ret = %d, useTable = %d, ver = %d\n", 
		table, index, ret, table->useVersion, ver);
	return ret;
}

static inline void TimeTableSet(TimeTable* table, Addr addr, Time time, Version ver) {
	int index = TimeTableGetIndex(addr, table->type);
	MSG(3, "TimeTableSet to addr 0x%llx with index %d\n", &(table->array[index]), index);
	MSG(3, "\t table addr = 0x%llx, array addr = 0x%llx\n", table, &(table->array[0]));
	table->array[index] = time;
	if (table->useVersion) {
		table->version[index] = ver;
	}
}


/*
 * SegTable:
 *
 */ 

static inline SegTable* SegTableAlloc() {
	SegTable* ret = (SegTable*) calloc(sizeof(SegTable), 1);

	int i;
	stat.nSegTableAllocated++;
	stat.nSegTableActive++;
	if (stat.nSegTableActive > stat.nSegTableActiveMax)
		stat.nSegTableActiveMax++;
	return ret;	
}

void SegTableFree(SegTable* table) {
	stat.nSegTableActive--;
	free(table);
}

static inline int SegTableGetIndex(Addr addr) {
	return ((UInt64)addr >> SEGTABLE_SHIFT) & SEGTABLE_MASK;
}

static inline SegEntry* SegEntryAlloc() {
	SegEntry* ret = calloc(sizeof(SegEntry), 1);
#if TIME_TABLE_TYPE == TIME_TABLE_VERSION
	int i;
	for (i=15; i<MAX_LEVEL; i++)
		ret->noBTV[i] = 1;
#endif
	return ret;
}

// convert 64 bit format into 32 bit format
TimeTable* convertTimeTable(TimeTable* table) {
	eventTimeTableConvertTo32();
	assert(table->type == TYPE_64BIT);
	TimeTable* ret = TimeTableAlloc(TYPE_32BIT, table->useVersion);
	int i;
	for (i=0; i<TIMETABLE_SIZE/2; i++) {
		ret->array[i*2] = ret->array[i];
		ret->array[i*2 + 1] = ret->array[i];
	}
	
	return ret;
	
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
	SegTable* segTable;
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
#if 0
	for (i=0; i<sTable.writePtr; i++) {
		ITableFree(sTable.entry[i].iTable);		
	}
#endif
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
	ret->addrHigh = highAddr;
	ret->segTable = SegTableAlloc();
	sTable.writePtr++;
	return ret;

}

typedef struct _CacheEntry {
	UInt64 tag;  
	UInt32 status;	
	Version version[2];
	int lastSize[2];	// required to know the region depth at eviction
	int type;

} CacheLine;

static CacheLine* tagTable;
static Table* valueTable[2];
//static Version* verTable;

static int lineNum;
static int lineShift;
static int lineMask;
static int bypassCache = 0;

static inline Bool isValid(CacheLine* entry) {
	return entry->status & STATUS_VALID;
}

static inline void setValid(CacheLine* entry) {
	entry->status |= STATUS_VALID;
}


static inline Time* getTimeAddr(int tableNum, int row, int index) {
	return TableGetElementAddr(valueTable[tableNum], row, index);
}


static inline int getLineIndex(Addr addr) {
	int nShift = 3; 	// 8 byte 
	int ret = (((UInt64)addr) >> nShift) & lineMask;
	assert(ret >= 0 && ret < lineNum);
	return ret;
}

static inline Version getCacheVersion(CacheLine* line) {
	//int index = lineIndex >> CACHE_VERSION_SHIFT;
	//return verTable[lineIndex];
	return line->version[0];
}

static inline void setCacheVersion(CacheLine* line, Version ver) {
	//int index = lineIndex >> CACHE_VERSION_SHIFT;
	//verTable[lineIndex] = ver;
	line->version[0] = ver;
}

static inline Bool isHit(CacheLine* entry, Addr addr) {
	MSG(3, "isHit addr = 0x%llx, tag = 0x%llx, entry tag = 0x%llx\n",
		addr, entry->tag, entry->tag);

	return ((entry->tag ^ (UInt64)addr) >> 3) == 0;
}

static inline CacheLine* getEntry(int index) {
	assert(index < lineNum);
	return &(tagTable[index]);
}

static inline int getFirstOnePosition(int input) {
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
	int lineSize = 8;	// 64bit granularity 
	lineNum = cacheSizeMB * 1024 * 1024 / lineSize;
	lineShift = getFirstOnePosition(lineNum);
	lineMask = lineNum - 1;
	fprintf(stderr, "MShadowCacheInit: total size: %d MB, lineNum %d, lineShift %d, lineMask 0x%x\n", 
		cacheSizeMB, lineNum, lineShift, lineMask);

	tagTable = calloc(sizeof(CacheLine), lineNum);
	valueTable[0] = TableCreate(lineNum, INIT_LEVEL);
	valueTable[1] = TableCreate(lineNum, INIT_LEVEL);

	//verTable = calloc(sizeof(Version), lineNum);

	MSG(3, "MShadowCacheInit: value Table created row %d col %d at addr 0x%x\n", 
		lineNum, INIT_LEVEL, valueTable[0]->array);
}

void MShadowCacheDeinit() {
	if (bypassCache == 1)
		return;

	//printStat();
	free(tagTable);
	TableFree(valueTable[0]);
	TableFree(valueTable[1]);
}

static inline Bool isValidVersion(Version prev, Version current) {
	if (current <= prev)
		return TRUE;
	else
		return FALSE;
}

#define MIN(a, b) ((a) < (b)? a : b)
#if 0
static inline Version getCachedVersion(CacheLine* line, int offset) {
	return line->version[offset];
}
#endif

static inline int getAccessWidthType(CacheLine* entry) {
	return TYPE_64BIT;
}

static inline Time getTimeLevel(SegEntry* segEntry, Index level, Addr addr, Version verCurrent, UInt32 type) {
	TimeTable* table = segEntry->tArray[level];
	Version version = segEntry->vArray[level];
	int useTableVer = segEntry->noBTV[level];

	Time ret = 0ULL;
	if (table == NULL) {
		ret = 0ULL;

	} else if (useTableVer) {
		MSG(0, "\t\t\tversion = %d, %d\n", version, verCurrent);
		assert(table->useVersion == 1);
		ret = TimeTableGet(table, addr, verCurrent);

	} else if (version == verCurrent) {
		assert(table->useVersion == 0);
		ret = TimeTableGet(table, addr, verCurrent);

	}
	return ret;
}

static inline void checkConvertTimeTable(SegEntry* segEntry, Index level, Version verCurrent, UInt32 type) {
		// check if a converting is required at the first access in a new region
		if (segEntry->vArray[level] < verCurrent) {

#if TIME_TABLE_TYPE == TIME_TABLE_ADAPTIVE
			// convert from BVT -> no BVT
			if (segEntry->nAccess[level] <= 8 && segEntry->noBTV[level] == 0) {
				segEntry->noBTV[level] = 1;	
				if (segEntry->tArray[level] != NULL) {
					TimeTableFree(segEntry->tArray[level]);
					segEntry->tArray[level] = NULL;
				}
			}

			// convert from no BVT -> BVT
			else if (segEntry->nAccess[level] > 8 && segEntry->noBTV[level] == 1) {
				segEntry->noBTV[level] = 0;	
				if (segEntry->tArray[level] != NULL) {
					TimeTableFree(segEntry->tArray[level]);
					segEntry->tArray[level] = NULL;
				}
			}
#endif
			segEntry->nAccess[level] = 0;
		}
		segEntry->nAccess[level]++;
}


static inline void setTimeLevel(SegEntry* segEntry, Index level, Addr addr, Version verCurrent, Time value, UInt32 type) {

		checkConvertTimeTable(segEntry, level, verCurrent, type);
		TimeTable* table = segEntry->tArray[level];
		Version verOld = segEntry->vArray[level];
		int useTableVersion = segEntry->noBTV[level];

		if (table == NULL) {
			table = TimeTableAlloc(type, useTableVersion);
			segEntry->tArray[level] = table; 
			TimeTableSet(table, addr, value, verCurrent);
		}

		// actual write
		else if (useTableVersion || verOld == verCurrent) {
			assert(table != NULL);
			TimeTableSet(table, addr, value, verCurrent);

		} else 	{
			TimeTable* toAdd = TimeTableAlloc(type, useTableVersion);
			TimeTableFree(table);
			segEntry->tArray[level] = toAdd; 
			TimeTableSet(toAdd, addr, value, verCurrent);
		} 
		segEntry->vArray[level] = verCurrent;
		assert(segEntry->tArray[level] != NULL);
}

static inline SegEntry* getSegEntry(Addr addr) {
	SEntry* sEntry = STableGetSEntry(addr);
	SegTable* segTable = sEntry->segTable;
	assert(segTable != NULL);
	int segIndex = SegTableGetIndex(addr);
	SegEntry* segEntry = segTable->entry[segIndex];
	if (segEntry == NULL) {
		segEntry = SegEntryAlloc();
		segTable->entry[segIndex] = segEntry;
		eventSegEntryAlloc();
	}
	return segEntry;
}

static inline int getStartInvalidLevel(CacheLine* line, Version* vArray, Index size) {
	Version oldVer = getCacheVersion(line);
	int firstInvalid = 0;

	if (oldVer == vArray[size-1])
		return size;

	int i;
	for (i=size-1; i>=0; i--) {
		if (oldVer >= vArray[i]) {
			firstInvalid = i+1;
			break;
		}
	}
	return firstInvalid;

}

static inline void MCacheValidateTag(CacheLine* line, int lineIndex, Version* vArray, Index size) {
	int firstInvalid = getStartInvalidLevel(line, vArray, size);
	Time* destAddr = getTimeAddr(0, lineIndex, firstInvalid);
	bzero(destAddr, sizeof(Time) * (size - firstInvalid));
	setCacheVersion(line, vArray[size-1]);
}

void MCacheEvict(CacheLine* cacheEntry, Addr addr, int size, Version* vArray) {
	//if (!isValid(cacheEntry))
	//	return;
	if (cacheEntry->tag == NULL)
		return;

	int row = getLineIndex(addr);
	Time* tArray = getTimeAddr(0, row, 0);

	int index;
	int lastValid = -1;
	int type = getAccessWidthType(cacheEntry);
	int startInvalid = getStartInvalidLevel(cacheEntry, vArray, size);

	MSG(0, "\tMCacheEvict 0x%llx, size=%d, effectiveSize=%d \n", addr, size, startInvalid);
//	fprintf(stderr, "\tMCacheEvict 0x%llx, size=%d startInvalid=%d\n", addr, size, startInvalid);
	SegEntry* segEntry = getSegEntry(addr);
	int i;
	for (i=0; i<startInvalid; i++) {
		eventEvict(i);
		setTimeLevel(segEntry, i, addr, vArray[i], tArray[i], type);
		MSG(0, "\t\toffset=%d, version=%d, value=%d\n", i, vArray[i], tArray[i]);
	}
#if 0
	for (i=1; i<startInvalid; i++) {
		Time valA = getTimeLevel(segEntry, i-1, addr, vArray[i-1], type);
		Time valB = getTimeLevel(segEntry, i, addr, vArray[i], type);

		if (valA < valB) {
			fprintf(stderr, "\tError at MCacheEvict 0x%llx, size %d offset = %d startInvalid = %d\n", addr, size, i, startInvalid);
			int j;
			for (j=1; j<startInvalid; j++) {
				Time val0 = getTimeLevel(segEntry, j-1, addr, vArray[j-1], type);
				Time val1 = getTimeLevel(segEntry, j, addr, vArray[j], type);

				TimeTable* table0 = segEntry->tArray[j-1];
				TimeTable* table1 = segEntry->tArray[j];
				if (j < startInvalid)
					assert(table0 != NULL);

				fprintf(stderr, "srcVal= [%d,%d], retVal = [%d, %d], ver[%d] = %d, %d\n", tArray[j-1], tArray[j], val0, val1, j, segEntry->vArray[j], vArray[j]);
				if (table1 != NULL)
					fprintf(stderr, "\t table useVersion = [%d %d] type = %d\n", table0->useVersion, table1->useVersion, table0->type);
			}
			assert(0);
		}
	}
#endif

	eventCacheEvict(size, index);
}

void MCacheFetch(CacheLine* entry, Addr addr, Index size, Version* vArray, Time* destAddr, int type) {
	MSG(0, "\tMCacheFetch 0x%llx, size %d \n", addr, size);

	SegEntry* segEntry = getSegEntry(addr);

	int startInvalid = -1;
	int i;
	for (i=0; i<size; i++) {
		destAddr[i] = getTimeLevel(segEntry, i, addr, vArray[i], type);
	}
#if 0
	for (i=1; i<size; i++) {
		if (destAddr[i-1] < destAddr[i]) {
			fprintf(stderr, "\tError at MCacheFetch 0x%llx, size %d offset = %d\n", addr, size, i);
			int j;
			for (j=0; j<size; j++) {
				fprintf(stderr, "val= %d, ver[%d] = %d, %d\n", destAddr[j], j, segEntry->vArray[j], vArray[j]);
				TimeTable* table = segEntry->tArray[j];
				fprintf(stderr, "\t table useVersion = %d type = %d\n", table->useVersion, table->type);
			}
		}
	}
#endif

	//setValid(entry);
}

static Time tempArray[1000];

Time* MShadowGetNoCache(Addr addr, Index size, Version* vArray, int type) {
	SegEntry* segEntry = getSegEntry(addr);	
	Index i;
	for (i=0; i<size; i++) {
		tempArray[i] = getTimeLevel(segEntry, i, addr, vArray[i], type);
	}

	return tempArray;	
}

void MShadowSetNoCache(Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	SegEntry* segEntry = getSegEntry(addr);	
	assert(segEntry != NULL);
	Index i;
	for (i=0; i<size; i++) {
		setTimeLevel(segEntry, i, addr, vArray[i], tArray[i], type);
	}
}

#ifdef DUMMY_SHADOW

Time* MShadowGet(Addr addr, Index size, Version* vArray, UInt32 width) {
	return tempArray;
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
}
#else

#ifdef NDEBUG
void check(Addr addr, Time* src, int size, int site) {}
#else
void check(Addr addr, Time* src, int size, int site) {
	int i;
	for (i=1; i<size; i++) {
		if (src[i-1] < src[i]) {
			fprintf(stderr, "site %d Addr 0x%llx size %d offset %d val=%lld %lld\n", site, addr, size, i, src[i-1], src[i]); 
			assert(0);
		}
	}
}
#endif

Time* MShadowGetCache(Addr addr, Index size, Version* vArray, int type) {
	int row = getLineIndex(addr);
	CacheLine* entry = getEntry(row);

	int offset = 0;
	Time* destAddr = getTimeAddr(offset, row, 0);


	if (isHit(entry, addr)) {
		eventReadHit();
		//MSG(0, "\t cache hit at 0x%llx firstInvalid = %d\n", destAddr, firstInvalid);
		MSG(3, "\t value0 %d value1 %d \n", destAddr[0], destAddr[1]);
		// check versions and if outdated, set to 0
		MSG(3, "\t tag validation \n", destAddr);
		//Version version = entry->version[offset];

		MCacheValidateTag(entry, row, vArray, size);
		check(addr, destAddr, size, 0);

	} else {
		// Unfortunately, this access results in a miss
		// 1. evict a line	
		eventReadEvict();
		//Addr evictAddr = getAddrFromTag(entry->tag, row);
		MSG(0, "\t eviction required \n", destAddr);
		MCacheEvict(entry, entry->tag, entry->lastSize[0], vArray);
		

		// 2. read line from MShadow to the evicted line
		MSG(3, "\t write values to cache \n", destAddr);
		MCacheFetch(entry, addr, size, vArray, destAddr, type);
		entry->tag = (UInt64)addr;
		check(addr, destAddr, size, 1);
	}

	entry->version[offset] = vArray[size-1];
	entry->lastSize[offset] = size;
	return destAddr;
}


void MShadowSetCache(Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	int row = getLineIndex(addr);
	int offset = 0;
#if 0
	if (type == TYPE_32BIT) {
		offset = ((UInt64)addr >> 2) & 0x1;
	}
#endif
	CacheLine* entry = getEntry(row);
	assert(row < lineNum);

	if (isHit(entry, addr)) {
		eventWriteHit();

		if (type == TYPE_32BIT && offset == 0 && 
			entry->type == TYPE_64BIT) {
			entry->version[1] = entry->version[0];
			entry->lastSize[1] = entry->lastSize[0];	
		}

	} else {
		eventWriteEvict();
		MCacheEvict(entry, entry->tag, entry->lastSize[0], vArray);
	} 		

	// copy Timestamps
	Time* destAddr = getTimeAddr(offset, row, 0);
	memcpy(destAddr, tArray, sizeof(Time) * size);
	setValid(entry);
	entry->type = type;
	entry->tag = addr;
	entry->version[offset] = vArray[size-1];
	entry->lastSize[offset] = size;

	check(addr, destAddr, size, 2);
}


Time* MShadowGet(Addr addr, Index size, Version* vArray, UInt32 width) {
	MSG(0, "MShadowGet 0x%llx, size %d \n", addr, size);
	//int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	int type = TYPE_64BIT;
	Addr tAddr = (Addr)((UInt64)addr & ~0x7);
	eventRead();
	if (bypassCache == 1) {
		return MShadowGetNoCache(tAddr, size, vArray, type);

	} else {
		return MShadowGetCache(tAddr, size, vArray, type);
	}
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	MSG(0, "MShadowSet 0x%llx, size %d \n", addr, size);
	
	//int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	int type = TYPE_64BIT;
	Addr tAddr = (Addr)((UInt64)addr & ~0x7);
	eventWrite();
	if (bypassCache == 1) {
		MShadowSetNoCache(tAddr, size, vArray, tArray, type);

	} else {
		MShadowSetCache(tAddr, size, vArray, tArray, type);
	}

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
	MShadowCacheInit(cacheSizeMB);
}


UInt MShadowDeinit() {
	printMemStat();
	STableDeinit();
	MShadowCacheDeinit();
}

#endif

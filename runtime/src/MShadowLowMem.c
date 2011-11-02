#include "defs.h"

//#define DUMMY_SHADOW

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "MemMapAllocator.h"

#include "debug.h"
#include "CRegion.h"
#include "MShadow.h"
#include "MShadowLow.h"
#include "compress.h"
#include "Table.h"
#include "uthash.h"
#include "CBuffer.h"
#include "config.h"

#include <string.h> // for memcpy

#define TIME_TABLE_BTV		0
#define TIME_TABLE_VERSION	1	
#define TIME_TABLE_ADAPTIVE	2

#define TIME_TABLE_TYPE		TIME_TABLE_BTV

#define WORD_SHIFT 2

// Cache Constatns and Parameters
#define STATUS_VALID	1
#define STATUS_DIRTY	2
#define STATUS_32BIT	3

// Timetable Type
#define TYPE_NOVERSION	0
#define TYPE_VERSION	1
#define TYPE_HYBRID		2
//#define TIMETABLE_TYPE	TYPE_VERSION

//#define CACHE_VERSION_SHIFT	0

/*
HASH_MAP_DEFINE_PROTOTYPES(ub,Addr,UInt8);
HASH_MAP_DEFINE_FUNCTIONS(ub,Addr,UInt8);

static hash_map_ub* activeLTables;

UInt64 addrHash(Addr addr) { return addr; }
int addrCompare(Addr a1, Addr a2) { return a1 == a2; }
*/

static int timetableType;
static STable sTable;

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
	UInt64 nLTableAlloc;
	UInt64 nTimeTableNewAlloc[100];
	UInt64 nTimeTableConvert[100];
	UInt64 nTimeTableRealloc[100];
	UInt64 nTimeTableReallocTotal;
	UInt64 nLevelWrite[100];
	UInt64 nEvict[100];
	UInt64 nEvictTotal;
	UInt64 nCacheEvictLevelTotal;
	UInt64 nCacheEvictLevelEffective;
	UInt64 nCacheEvict;
	UInt64 nGC;
	UInt64 nLTableAccess;

	// tracking overhead of timetables (in bytes) with compression
	UInt64 timeTableOverhead;
	UInt64 timeTableOverheadMax;

#if COMPRESSION_POLICY == 1
	UInt64 nActiveTableHits;
	UInt64 nActiveTableMisses;
#endif
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

static L1Stat cacheStat;


static double getSizeMB(UInt64 nUnit, UInt64 size) {
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

	fprintf(stderr, "\tnGC = %d\n", stat.nGC);

#if COMPRESSION_POLICY == 1
	double activeTableHitRate = (double)stat.nActiveTableHits / (double)(stat.nActiveTableHits + stat.nActiveTableMisses);
	fprintf(stderr, "\t'active LTable list' hit ratio = %.2f (hits = %llu, misses = %llu)\n", activeTableHitRate,stat.nActiveTableHits,stat.nActiveTableMisses);
#endif
}


static void printMemReqStat() {
	//fprintf(stderr, "Overall allocated = %d, converted = %d, realloc = %d\n", 
	//	totalAlloc, totalConvert, totalRealloc);
	double segSize = getSizeMB(stat.nSegTableActiveMax, sizeof(SegTable));
	double lTableSize = getSizeMB(stat.nLTableAlloc, sizeof(LTable));

	// XXX FIXME: nTable0 is unused and nTable1 is probably used incorrectly!
	UInt64 nTable0 = stat.nTimeTableAllocated[0] - stat.nTimeTableFreed[0];
	int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TIMETABLE_SIZE / 2);
	UInt64 nTable1 = stat.nTimeTableAllocated[1] - stat.nTimeTableFreed[1];
	int sizeTable32 = sizeof(TimeTable) + sizeof(Time) * TIMETABLE_SIZE;
	UInt64 sizeUncompressed = stat.nTimeTableActive * sizeof(Time) * TIMETABLE_SIZE / 2;
	double tTableSize = getSizeMB(sizeUncompressed, 1);
	//double tTableSize1 = getSizeMB(nTable1, sizeTable32);
	//double tTableSize = tTableSize0 + tTableSize1;

	double tTableSizeWithCompression = getSizeMB(stat.timeTableOverheadMax, 1);


	int sizeVersion64 = sizeof(Version) * (TIMETABLE_SIZE / 2);
	int sizeVersion32 = sizeof(Version) * (TIMETABLE_SIZE);
	UInt64 nVTable0 = stat.nVersionTableAllocated[0] - stat.nVersionTableFreed[0];
	UInt64 nVTable1 = stat.nVersionTableAllocated[1] - stat.nVersionTableFreed[1];
	double vTableSize0 = getSizeMB(nVTable0, sizeVersion64);
	double vTableSize1 = getSizeMB(nVTable1, sizeVersion32);
	double vTableSize = vTableSize0 + vTableSize1;


	double cacheSize = getCacheSize(getMaxActiveLevel());
	double totalSize = segSize + lTableSize + tTableSize + cacheSize;
	double totalSizeComp = segSize + lTableSize + cacheSize + tTableSizeWithCompression;
	
	UInt64 compressedNoBuffer = stat.timeTableOverheadMax - getCBufferSize() * sizeTable64;
	UInt64 noCompressedNoBuffer = sizeUncompressed - getCBufferSize() * sizeTable64;
	double compressionRatio = (double)noCompressedNoBuffer / compressedNoBuffer;

	//minTotal += getCacheSize(2);
	fprintf(stderr, "%lld, %lld, %lld\n", stat.timeTableOverhead, sizeUncompressed, stat.timeTableOverhead - sizeUncompressed);
	fprintf(stderr, "\nRequired Memory Analysis\n");
	fprintf(stderr, "\tShadowMemory (SegTable / TTable / TTableCompressed / VTable) = %.2f / %.2f / %.2f / %.2f\n",
		segSize + lTableSize, tTableSize, tTableSizeWithCompression, vTableSize);
	fprintf(stderr, "\tReqMemSize (Total / Cache / Uncompressed Shadow / Compressed Shadow) = %.2f / %.2f / %.2f / %.2f\n",
		totalSize, cacheSize, segSize + tTableSize + vTableSize, segSize + tTableSizeWithCompression + vTableSize);  
	fprintf(stderr, "\tTagTable (Uncompressed / Compressed / Ratio / Comp Ratio) = %.2f / %.2f / %.2f / %.2f\n",
		tTableSize, tTableSizeWithCompression, tTableSize / tTableSizeWithCompression, compressionRatio);
	fprintf(stderr, "\tTotal (Uncompressed / Compressed / Ratio) = %.2f / %.2f / %.2f\n",
		totalSize, totalSizeComp, totalSize / totalSizeComp);
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

		int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TIMETABLE_SIZE / 2);
		double sizeSegTable = getSizeMB(stat.nSegTableNewAlloc[i], sizeof(SegTable));
		double sizeTimeTable = getSizeMB(stat.nTimeTableNewAlloc[i], sizeTable64);
		double sizeVersionTable = getSizeMB(stat.nTimeTableConvert[i], sizeof(TimeTable));
		double sizeLevel = sizeSegTable + sizeTimeTable + sizeVersionTable;
		double reallocPercent = (double)stat.nTimeTableRealloc[i] * 100.0 / (double)stat.nEvict[i];

		if (i < 2)
			minTotal += sizeLevel;
		
		fprintf(stderr, "\tLevel [%2d] Wr Cnt = %lld, TTable=%.2f, VTable=%.2f Sum=%.2f MB\n", 
			i, stat.nLevelWrite[i], sizeTimeTable, sizeVersionTable, sizeLevel, sizeLevel);
		fprintf(stderr, "\t\tReallocPercent=%.2f, Evict=%lld, Realloc=%lld\n",
			reallocPercent, stat.nEvict[i], stat.nTimeTableRealloc[i]);
			
	}
}

static void printMemStatAllocation() {
	fprintf(stderr, "\nShadow Memory Allocation Stats\n");
	fprintf(stderr, "\tnSTableEntry = %d\n", stat.nSTableEntry);
	fprintf(stderr, "\tnSegTable: Alloc / Active / ActiveMax = %lld / %lld / %lld\n",
		 stat.nSegTableAllocated, stat.nSegTableActive, stat.nSegTableActiveMax);
	fprintf(stderr, "\tnTimeTable(type %d): Alloc / Freed / ActiveMax = %lld, %lld / %lld, %lld / %lld\n",
		 timetableType, stat.nTimeTableAllocated[0], stat.nTimeTableAllocated[1],
		 stat.nTimeTableFreed[0], stat.nTimeTableFreed[1],
		 stat.nTimeTableActiveMax);
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

static inline void eventLTableAlloc() {
	stat.nLTableAlloc++;
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

static inline void eventLevelWrite(int level) {
	stat.nLevelWrite[level]++;
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

static inline void TimeTableClean(TimeTable* table) {
	int size = TimeTableEntrySize(table->type);
	bzero(table->array, sizeof(Time) * size);

	if (table->useVersion) {
		bzero(table->version, sizeof(Version) * size);
	}
}

static inline void TimeTableUpdateOverhead(int size) {
	stat.timeTableOverhead += size; 
	if(stat.timeTableOverhead > stat.timeTableOverheadMax)
		stat.timeTableOverheadMax = stat.timeTableOverhead;
#if 0
	if (getCompression() == 0) {
		//assert(stat.timeTableOverhead == stat.nTimeTableActive * sizeof(Time) * TIMETABLE_SIZE / 2);
		//assert(stat.timeTableOverheadMax == stat.nTimeTableActiveMax * sizeof(Time) * TIMETABLE_SIZE / 2);
		UInt64 sizeUncompressed = stat.nTimeTableActive * sizeof(Time) * TIMETABLE_SIZE / 2;
		fprintf(stderr, "%lld, %lld, %lld\n", stat.timeTableOverhead, sizeUncompressed, stat.timeTableOverhead - sizeUncompressed);
	}
#endif
}

static inline TimeTable* TimeTableAlloc(int sizeType, int useVersion) {
	assert(sizeType == TYPE_32BIT || sizeType == TYPE_64BIT);
	int size = TimeTableEntrySize(sizeType);
	TimeTable* ret = malloc(sizeof(TimeTable));
	ret->array = MemPoolAlloc();
	bzero(ret->array, sizeof(Time) * size);

	ret->type = sizeType;
	ret->useVersion = useVersion;
	ret->size = sizeof(Time) * TIMETABLE_SIZE / 2;

	stat.nTimeTableAllocated[sizeType]++;
	stat.nTimeTableActive++;
	if (stat.nTimeTableActive > stat.nTimeTableActiveMax)
		stat.nTimeTableActiveMax++;
	
	TimeTableUpdateOverhead(ret->size);

	if (useVersion == 1) {
		ret->version = calloc(size,sizeof(Version));
		stat.nVersionTableAllocated[sizeType]++;
	}
	return ret;
}


static inline void TimeTableFree(TimeTable* table, UInt8 isCompressed) {
	stat.nTimeTableActive--;
	int sizeType = table->type;
	assert(sizeType == TYPE_32BIT || sizeType == TYPE_64BIT);
	stat.nTimeTableFreed[table->type]++;
	MemPoolFree(table->array);
	if (table->useVersion) {
		assert(table->version != NULL);
		free(table->version);
		stat.nVersionTableFreed[table->type]++;
	}
	TimeTableUpdateOverhead(table->size * -1);
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
	assert(table != NULL);
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
	return ret;
}

static inline void TimeTableSet(TimeTable* table, Addr addr, Time time, Version ver) {
	assert(table != NULL);
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
	SegTable* ret = (SegTable*) calloc(1,sizeof(SegTable));

	int i;
	stat.nSegTableAllocated++;
	stat.nSegTableActive++;
	if (stat.nSegTableActive > stat.nSegTableActiveMax)
		stat.nSegTableActiveMax++;
	return ret;	
}

static void SegTableFree(SegTable* table) {
	stat.nSegTableActive--;
	free(table);
}

static inline int SegTableGetIndex(Addr addr) {
	return ((UInt64)addr >> SEGTABLE_SHIFT) & SEGTABLE_MASK;
}

static inline LTable* LTableAlloc() {
	LTable* ret = calloc(1,sizeof(LTable));
#if TIME_TABLE_TYPE == TIME_TABLE_VERSION
	int i;
	for (i=15; i<MAX_LEVEL; i++)
		ret->noBTV[i] = 1;
#endif
	return ret;
}

// convert 64 bit format into 32 bit format
static TimeTable* convertTimeTable(TimeTable* table) {
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


static void STableInit() {
	sTable.writePtr = 0;
}

static void STableDeinit() {
	int i;
#if 0
	for (i=0; i<sTable.writePtr; i++) {
		ITableFree(sTable.entry[i].iTable);		
	}
#endif
}

void checkOffsets(Version* vArray, Index max_level) {
	//fprintf(stderr,"begin checkOffsets(level = %u)\n",max_level);

	UInt64 nMatches[MAX_LEVEL]; // number of total matches
	UInt64 nChecks[MAX_LEVEL]; // number of potential matches

	int i;
	for(i = 0; i < (int)max_level-1; ++i) {
		nMatches[i] = 0;
		nChecks[i] = 0;
	}

	for(i = 0; i < sTable.writePtr; ++i) {
		//fprintf(stderr,"\ti = %d\n",i);
		SegTable* currSeg = sTable.entry[i].segTable;

		int j;
		for(j = 0; j < SEGTABLE_SIZE; ++j) {
			LTable* currLT = currSeg->entry[j];
			if(currLT != NULL) {
				//fprintf(stderr,"\tj = %d\n",j);

				int k;
				for(k = 0; k < (int)max_level-1; ++k) {
					// only do check if versions are valid
					if(currLT->vArray[k] == vArray[k] && currLT->vArray[k+1] == vArray[k+1]) {
						//fprintf(stderr,"\tk = %d\n",k);

						TimeTable* tt1 = (TimeTable*)currLT->tArray[k];
						TimeTable* tt2 = (TimeTable*)currLT->tArray[k+1];

						// run through each entry in tt2 and see if it's
						// zero OR constant offset from tt1
						UInt8 match = 1;

						int m;
						int offset = -1;
						for(m = 0; m < TIMETABLE_SIZE/2; ++m) {
							if(tt1->array[m] != 0) {
								int currOffset = tt1->array[m] - tt2->array[m];
								if(offset == -1) {
									offset = currOffset;
								}
								else if(offset != currOffset) {
									match = 0;
									break;
								}
							}
						}

						nChecks[k]++;
						if(match) { nMatches[k]++; }
					}
				}
			}
		}
	}

	if(max_level > 1) fprintf(stderr,"Current Match Ratios:\n");

	double matchRatio;
	for(i = 0; i < (int)max_level-1; ++i) {
		matchRatio = (double)nMatches[i] / (double)nChecks[i];
		fprintf(stderr,"\t%d-%d: %.4f (%llu / %llu)\n",i,i+1,matchRatio,nMatches[i],nChecks[i]);
	}
	//fprintf(stderr,"end checkOffsets(level = %u)\n",max_level);
}

static SEntry* STableGetSEntry(Addr addr) {
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
	Addr tag;  
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

static inline Time* getTimeAddr(int tableNum, int row, int index) {
	return TableGetElementAddr(valueTable[tableNum], row, index);
}

static inline int getLineIndex(Addr addr) {
#if 0
	int nShift = 3; 	// 8 byte 
	int ret = (((UInt64)addr) >> nShift) & lineMask;
	assert(ret >= 0 && ret < lineNum);
#endif
	int nShift = 3;	
	int val0 = (((UInt64)addr) >> nShift) & lineMask;
	int val1 = (((UInt64)addr) >> (nShift + lineShift)) & lineMask;
	return val0 ^ val1;
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

	return (((UInt64)entry->tag ^ (UInt64)addr) >> 3) == 0;
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

static void MCacheInit(int cacheSizeMB) {
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

	tagTable = calloc(lineNum,sizeof(CacheLine));
	valueTable[0] = TableCreate(lineNum, getRegionDepth());
	//valueTable[1] = TableCreate(lineNum, getRegionDepth());

	//verTable = calloc(lineNum,sizeof(Version));

	MSG(3, "MShadowCacheInit: value Table created row %d col %d at addr 0x%x\n", 
		lineNum, getRegionDepth(), valueTable[0]->array);
}

static void MCacheDeinit() {
	if (bypassCache == 1)
		return;

	//printStat();
	free(tagTable);
	TableFree(valueTable[0]);
	//TableFree(valueTable[1]);
}


static inline int getAccessWidthType(CacheLine* entry) {
	return TYPE_64BIT;
}

static inline Time getTimeLevel(LTable* lTable, Index level, Addr addr, Version verCurrent, UInt32 type) {
	TimeTable* table = (TimeTable*)lTable->tArray[level];
	Version version = lTable->vArray[level];
	int useTableVer = lTable->noBTV[level]; // check if version is in timetable (1) or ltable (0)

	Time ret = 0ULL;
	if (table == NULL) {
		ret = 0ULL;

	} else if (useTableVer || version == verCurrent) {
		MSG(0, "\t\t\tversion = %d, %d\n", version, verCurrent);
		ret = TimeTableGet(table, addr, verCurrent);

	} 
	return ret;
}

static inline Version LTableGetVer(LTable* lTable, Index level) {
	return lTable->vArray[level];
}

static inline void LTableSetVer(LTable* lTable, Index level, Version ver) {
	lTable->vArray[level] = ver;
}

static inline TimeTable* LTableGetTable(LTable* lTable, Index level) {
	return lTable->tArray[level];
}

static inline void LTableSetTable(LTable* lTable, Index level, TimeTable* table) {
	lTable->tArray[level] = table;
}

static inline int findLowestInvalidIndex(LTable* lTable, Version* vArray) {
	int lowestInvalidIndex = 0;
	while(lowestInvalidIndex < MAX_LEVEL 
		&& lTable->tArray[lowestInvalidIndex] != NULL 
		&& lTable->vArray[lowestInvalidIndex] >= vArray[lowestInvalidIndex]) {
		++lowestInvalidIndex;
	}

	return lowestInvalidIndex;
}

static inline void cleanTimeTables(LTable* lTable, Index start) {
	Index i;
	for(i = start; i < MAX_LEVEL; ++i) {
		TimeTable* time = lTable->tArray[i];
		if (time != NULL) {
			//fprintf(stderr, "(%d)\t", i);
			TimeTableFree(time,lTable->isCompressed);
			lTable->tArray[i] = NULL;
		}
	}
}

static inline void gcLevelUnknownSize(LTable* lTable, Version* vArray) {
	int lii = findLowestInvalidIndex(lTable,vArray);
	cleanTimeTables(lTable,lii);
}

void gcLevel(LTable* table, Version* vArray, int size) {
	//fprintf(stderr, "%d: \t", size);
	int i;
	for (i=0; i<size; i++) {
		TimeTable* time = table->tArray[i];
		if (time == NULL)
			continue;

		Version ver = table->vArray[i];
		if (ver < vArray[i]) {
			// out of date
			//fprintf(stderr, "(%d, %d %d)\t", i, ver, vArray[i]);
			TimeTableFree(time,table->isCompressed);
			table->tArray[i] = NULL;
		}
	}

	cleanTimeTables(table,size);
}

static void gcStart(Version* vArray, int size) {
	int i, j;
	stat.nGC++;
	for (i=0; i<STABLE_SIZE; i++) {
		SegTable* table = sTable.entry[i].segTable;	
		if (table == NULL)
			continue;
		
		for (j=0; j<SEGTABLE_SIZE; j++) {
			LTable* lTable = table->entry[j];
			if (lTable != NULL) {
				gcLevel(lTable, vArray, size);
			}
		}
	}
}
static inline void checkConvertTimeTable(LTable* lTable, Index level, Version verCurrent, UInt32 type) {
		Version verOld = LTableGetVer(lTable, level);
		TimeTable* table = LTableGetTable(lTable, level);

		// check if a converting is required at the first access in a new region
		if (verOld < verCurrent) {

#if TIME_TABLE_TYPE == TIME_TABLE_ADAPTIVE
			// convert from BVT -> no BVT
			if (lTable->nAccess[level] <= 8 && lTable->noBTV[level] == 0) {
				lTable->noBTV[level] = 1;	
				if (table != NULL) {
					TimeTableFree(table,lTable->isCompressed);
					LTableSetTable(lTable, level, NULL);
				}
			}

			// convert from no BVT -> BVT
			else if (lTable->nAccess[level] > 8 && lTable->noBTV[level] == 1) {
				lTable->noBTV[level] = 0;	
				if (table != NULL) {
					TimeTableFree(table,lTable->isCompressed);
					LTableSetTable(lTable, level, NULL);
				}
			}
#endif
			lTable->nAccess[level] = 0;
		}
		lTable->nAccess[level]++;
}

static inline void setTimeLevel(LTable* lTable, Index level, Addr addr, Version verCurrent, Time value, UInt32 type) {

	checkConvertTimeTable(lTable, level, verCurrent, type);
	TimeTable* table = LTableGetTable(lTable, level);
	Version verOld = LTableGetVer(lTable, level);
	int useTableVersion = lTable->noBTV[level];
	assert(useTableVersion == 0);
	eventLevelWrite(level);

	// no timeTable exists so create it
	if (table == NULL) {
		eventTimeTableNewAlloc(level);
		table = TimeTableAlloc(type, useTableVersion);

		LTableSetTable(lTable, level, table); 
		TimeTableSet(table, addr, value, verCurrent);
	}

	// A table exists and is correct version, so use it
	else if (useTableVersion || verOld == verCurrent) {
		assert(table != NULL);
		TimeTableSet(table, addr, value, verCurrent);
	}

	// exists but version is old so clean it and reuse
	else {
		TimeTableClean(table);
		TimeTableSet(table, addr, value, verCurrent);
	} 
	LTableSetVer(lTable, level, verCurrent);
}


static inline LTable* getLTable(Addr addr, Version* vArray) {
	SEntry* sEntry = STableGetSEntry(addr);
	SegTable* segTable = sEntry->segTable;
	assert(segTable != NULL);
	stat.nLTableAccess++;
	int segIndex = SegTableGetIndex(addr);
	LTable* lTable = segTable->entry[segIndex];
	if (lTable == NULL) {
		lTable = LTableAlloc();
		if (getCompression()) {
			int compressGain = CBufferAdd(lTable);
			TimeTableUpdateOverhead(compressGain * -1);
		}
		segTable->entry[segIndex] = lTable;
		eventLTableAlloc();
	}

	if(lTable->isCompressed) {
		gcLevelUnknownSize(lTable,vArray);
		int loss = decompressLTable(lTable);
		int gain = CBufferAdd(lTable);
		TimeTableUpdateOverhead(loss - gain);
	}

	return lTable;
}

static inline int getStartInvalidLevel(Version oldVer, Version* vArray, Index size) {
	int firstInvalid = 0;

	if (size > 2)
		MSG(0, "\tMCacheEvict oldVer = %lld, newVer = %lld %lld \n", oldVer, vArray[size-2], vArray[size-1]);

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

static void check(Addr addr, Time* src, int size, int site) {
#ifndef NDEBUG
	int i;
	for (i=1; i<size; i++) {
		if (src[i-1] < src[i]) {
			fprintf(stderr, "site %d Addr 0x%llx size %d offset %d val=%lld %lld\n", 
				site, addr, size, i, src[i-1], src[i]); 
			assert(0);
		}
	}
#endif
}

static inline int hasVersionError(Version* vArray, int size) {
#ifndef NDEBUG
	int i;
	for (i=1; i<size; i++) {
		if (vArray[i-1] > vArray[i])
			return 1;
	}
#endif
	return 0;
}


static inline void MCacheValidateTag(CacheLine* line, Time* destAddr, Version* vArray, Index size) {
	int firstInvalid = getStartInvalidLevel(line->version[0], vArray, size);

	MSG(0, "\t\tMCacheValidateTag: invalid from level %d\n", firstInvalid);
	if (size > firstInvalid)
		bzero(&destAddr[firstInvalid], sizeof(Time) * (size - firstInvalid));
}

static void MCacheEvict(Time* tArray, Addr addr, int size, Version oldVersion, Version* vArray, int type) {
	if (addr == NULL)
		return;

	int i;
	int lastValid = -1;
	int startInvalid = getStartInvalidLevel(oldVersion, vArray, size);

	MSG(0, "\tMCacheEvict 0x%llx, size=%d, effectiveSize=%d \n", addr, size, startInvalid);
		
	LTable* lTable = getLTable(addr,vArray);
	for (i=0; i<startInvalid; i++) {
		eventEvict(i);
		if (tArray[i] == 0ULL) {
			break;
		}
		setTimeLevel(lTable, i, addr, vArray[i], tArray[i], type);
		MSG(0, "\t\toffset=%d, version=%d, value=%d\n", i, vArray[i], tArray[i]);
	}
	eventCacheEvict(size, startInvalid);

	CBufferAccess(lTable);
}

static void MCacheFetch(Addr addr, Index size, Version* vArray, Time* destAddr, int type) {
	MSG(0, "\tMCacheFetch 0x%llx, size %d \n", addr, size);
	LTable* lTable = getLTable(addr,vArray);

	int startInvalid = -1;
	int i;
	for (i=0; i<size; i++) {
		destAddr[i] = getTimeLevel(lTable, i, addr, vArray[i], type);
	}

	CBufferAccess(lTable);
}

static Time tempArray[1000];

static Time* MNoCacheGet(Addr addr, Index size, Version* vArray, int type) {
	LTable* lTable = getLTable(addr,vArray);	
	Index i;
	for (i=0; i<size; i++) {
		tempArray[i] = getTimeLevel(lTable, i, addr, vArray[i], type);
	}

	CBufferAccess(lTable);
	return tempArray;	
}

static void MNoCacheSet(Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	LTable* lTable = getLTable(addr,vArray);	
	assert(lTable != NULL);
	Index i;
	for (i=0; i<size; i++) {
		setTimeLevel(lTable, i, addr, vArray[i], tArray[i], type);
	}

	CBufferAccess(lTable);
}


static Time* MCacheGet(Addr addr, Index size, Version* vArray, int type) {
	int row = getLineIndex(addr);
	CacheLine* entry = getEntry(row);

	int offset = 0;
	Time* destAddr = getTimeAddr(offset, row, 0);

	if (isHit(entry, addr)) {
		eventReadHit();
		//MSG(0, "\t cache hit at 0x%llx firstInvalid = %d\n", destAddr, firstInvalid);
		MCacheValidateTag(entry, destAddr, vArray, size);
		check(addr, destAddr, size, 0);

	} else {
		// Unfortunately, this access results in a miss
		// 1. evict a line	
		eventReadEvict();
		int evictSize = entry->lastSize[0];
		if (size < evictSize)
			evictSize = size;

		MSG(0, "\t CacheGet: evict size = %d, lastSize = %d, size = %d\n", 
			evictSize, entry->lastSize[0], size);
		//MCacheEvict(destAddr, entry->tag, size, entry->version[0], vArray, entry->type);
		MCacheEvict(destAddr, entry->tag, evictSize, entry->version[0], vArray, entry->type);

		// 2. read line from MShadow to the evicted line
		MCacheFetch(addr, size, vArray, destAddr, type);
		entry->tag = addr;
		check(addr, destAddr, size, 1);
	}

	entry->version[0] = vArray[size-1];
	if (size > entry->lastSize[0])
		MSG(0, "\t CacheGet: size increased from %d to %d at addr 0x%llx\n", entry->lastSize[0], size, addr);
	entry->lastSize[0] = size;
	return destAddr;
}


static void MCacheSet(Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	int row = getLineIndex(addr);
	int offset = 0;
	Time* destAddr = getTimeAddr(offset, row, 0);
#if 0
	if (type == TYPE_32BIT) {
		offset = ((UInt64)addr >> 2) & 0x1;
	}
#endif
	CacheLine* entry = getEntry(row);
	assert(row < lineNum);

	if (hasVersionError(vArray, size)) {
		assert(0);
	}

	if (isHit(entry, addr)) {
		eventWriteHit();
#if 0
		if (type == TYPE_32BIT && offset == 0 && 
			entry->type == TYPE_64BIT) {
			entry->version[1] = entry->version[0];
			entry->lastSize[1] = entry->lastSize[0];	
		}
#endif

	} else {
		eventWriteEvict();

		int evictSize = entry->lastSize[0];
		if (size < evictSize)
			evictSize = size;

		MSG(0, "\t CacheSet: evict size = %d, lastSize = %d, size = %d\n", evictSize, entry->lastSize[0], size);
		MCacheEvict(destAddr, entry->tag, evictSize, entry->version[0], vArray, entry->type);
	} 		

	// copy Timestamps
	memcpy(destAddr, tArray, sizeof(Time) * size);
	entry->type = type;
	entry->tag = addr;
	entry->version[0] = vArray[size-1];
	entry->lastSize[0] = size;

	check(addr, destAddr, size, 2);
}



static Time* _MShadowGetCache(Addr addr, Index size, Version* vArray, UInt32 width) {
	if (size < 1)
		return NULL;

	//int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	int type = TYPE_64BIT;
	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "MShadowGet 0x%llx, size %d \n", tAddr, size);
	eventRead();
	if (bypassCache == 1) {
		return MNoCacheGet(tAddr, size, vArray, type);

	} else {
		return MCacheGet(tAddr, size, vArray, type);
	}
}

static UInt64 nextGC = 1024;
static int gcPeriod = -1;

void setGCPeriod(int time) {
	fprintf(stderr, "[kremlin] set GC period to %d\n", time);
	nextGC = time;
	gcPeriod = time;
	if (time == 0)
		nextGC = 0xFFFFFFFFFFFFFFFF;
}

static void _MShadowSetCache(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	MSG(0, "MShadowSet 0x%llx, size %d \n", addr, size);
	if (size < 1)
		return;

	if (stat.nTimeTableActiveMax >= nextGC) {
		gcStart(vArray, size);
		//nextGC = stat.nTimeTableActive + gcPeriod;
		nextGC += gcPeriod;
	}

	//int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	int type = TYPE_64BIT;
	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "MShadowSet 0x%llx, size %d \n", tAddr, size);
	eventWrite();
	if (bypassCache == 1) {
		MNoCacheSet(tAddr, size, vArray, tArray, type);

	} else {
		MCacheSet(tAddr, size, vArray, tArray, type);
	}
}


/*
 * Init / Deinit
 */
UInt MShadowInitCache(int cacheSizeMB, int type) {
	fprintf(stderr,"[kremlin] MShadow Init with cache %d MB, TimeTableType = %d, TimeTableSize = %d\n",
		cacheSizeMB, type, sizeof(TimeTable));

	int size = TimeTableEntrySize(TYPE_64BIT);
	MemPoolInit(1024, size * sizeof(Time));
	
	timetableType = type;
	if (gcPeriod == -1)
		setGCPeriod(1024);

	cacheMB = cacheSizeMB;
	STableInit();
	MCacheInit(cacheSizeMB);

	CBufferInit(getCBufferSize());
	MShadowGet = _MShadowGetCache;
	MShadowSet = _MShadowSetCache;
}


UInt MShadowDeinitCache() {
	CBufferDeinit();
	printMemStat();
	STableDeinit();
	MCacheDeinit();
	
}



#include "kremlin.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "MShadowCache.h"
#include "MemMapAllocator.h"
#include "debug.h"
#include "CRegion.h"
#include "MShadowSkadu.h"
#include "MShadowStat.h"
#include "Table.h"
#include "uthash.h"
#include "compression.h"
#include "config.h"

#include <string.h> // for memcpy

#define WORD_SHIFT 2


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


/*
 * Compression Configuration
 * instead of calling KConfigGetCompression repeatedly, 
 * store the result and reuse.
 */
static int _useCompression = 0;
static inline int useCompression() {
	return _useCompression;
}

static void setCompression() {
	_useCompression = KConfigGetCompression();
}



/*
 * TimeTable: simple array of Time with TIMETABLE_SIZE elements
 *
 */ 

static int TimeTableEntrySize(int type) {
	int size = TIMETABLE_SIZE;
	if (type == TYPE_64BIT)
		size >>= 1;
	return size;
}

static inline void TimeTableClean(TimeTable* table) {
	int size = TimeTableEntrySize(table->type);
	bzero(table->array, sizeof(Time) * size);
}

static inline TimeTable* TimeTableAlloc(int sizeType) {
	assert(sizeType == TYPE_32BIT || sizeType == TYPE_64BIT);
	int size = TimeTableEntrySize(sizeType);
	TimeTable* ret = MemPoolAllocSmall(sizeof(TimeTable));
	ret->array = MemPoolAlloc();
	bzero(ret->array, sizeof(Time) * size);

	ret->type = sizeType;
	ret->size = sizeof(Time) * TIMETABLE_SIZE / 2;

	eventTimeTableAlloc(sizeType, ret->size);
	return ret;
}


static inline void TimeTableFree(TimeTable* table, UInt8 isCompressed) {
	eventTimeTableFree(table->type, table->size);
	int sizeType = table->type;
	assert(sizeType == TYPE_32BIT || sizeType == TYPE_64BIT);
	MemPoolFree(table->array);
	MemPoolFreeSmall(table, sizeof(TimeTable));
}


static inline int TimeTableGetIndex(Addr addr, int type) {
    int ret = ((UInt64)addr >> WORD_SHIFT) & TIMETABLE_MASK;
	assert(ret < TIMETABLE_SIZE);
	if (type == TYPE_64BIT)
		ret >>= 1;

	return ret;
}

// note that TimeTableGet is not affected by access width
static inline Time TimeTableGet(TimeTable* table, Addr addr, Version ver, UInt32 type) {
	assert(table != NULL);
	int index = TimeTableGetIndex(addr, table->type);
	Time ret = 0ULL;
	ret = table->array[index];
	return ret;
}

static inline void TimeTableSet(TimeTable* table, Addr addr, Time time, Version ver, UInt32 type) {
	assert(table != NULL);
	int index = TimeTableGetIndex(addr, table->type);

	MSG(3, "TimeTableSet to addr 0x%llx with index %d\n", &(table->array[index]), index);
	MSG(3, "\t table addr = 0x%llx, array addr = 0x%llx\n", table, &(table->array[0]));

	table->array[index] = time;
	if (table->type == TYPE_32BIT && type == TYPE_64BIT) {
		table->array[index+1] = time;
	}

}


/*
 * SegTable:
 *
 */ 

static inline SegTable* SegTableAlloc() {
	SegTable* ret = (SegTable*) MemPoolCallocSmall(1,sizeof(SegTable));
	eventSegTableAlloc();
	return ret;	
}

static void SegTableFree(SegTable* table) {
	eventSegTableFree();
	MemPoolFreeSmall(table, sizeof(SegTable));
}

static inline int SegTableGetIndex(Addr addr) {
	return ((UInt64)addr >> SEGTABLE_SHIFT) & SEGTABLE_MASK;
}


static inline LTable* LTableAlloc() {
	LTable* ret = MemPoolCallocSmall(1,sizeof(LTable));
	ret->code = 0xDEADBEEF;
	return ret;
}

// convert 64 bit format into 32 bit format
static TimeTable* TimeTableConvert(TimeTable* table) {
	eventTimeTableConvertTo32();
	assert(table->type == TYPE_64BIT);
	TimeTable* ret = TimeTableAlloc(TYPE_32BIT);
	int i;
	for (i=0; i<TIMETABLE_SIZE/2; i++) {
		ret->array[i*2] = ret->array[i];
		ret->array[i*2 + 1] = ret->array[i];
	}
	return ret;
}


/*
 * STable: introduced to support 64-bit address space
 *
 */
static void STableInit() {
	sTable.writePtr = 0;
}

static void STableDeinit() {
	int i;
	for (i=0; i<sTable.writePtr; i++) {
		SEntry* entry = &sTable.entry[i];
		SegTableFree(entry->segTable);		
	}
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

	SEntry* ret = &sTable.entry[sTable.writePtr];
	ret->addrHigh = highAddr;
	ret->segTable = SegTableAlloc();
	sTable.writePtr++;
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

static inline Time LTableGetTime(LTable* lTable, Index level, Addr addr, Version verCurrent, UInt32 type) {
	TimeTable* table = LTableGetTable(lTable, level);
	Version version = lTable->vArray[level];

	Time ret = 0ULL;
	if (table == NULL) {
		ret = 0ULL;

	} else if (version == verCurrent) {
		ret = TimeTableGet(table, addr, verCurrent, type);
		MSG(0, "\t\tlv %d: \tversion = [%d, %d] value = %d\n", level, version, verCurrent, ret);
	} 

	return ret;
}

static inline void LTableSetTime(LTable* lTable, Index level, Addr addr, Version verCurrent, Time value, UInt32 type) {
	TimeTable* table = LTableGetTable(lTable, level);
	Version verOld = LTableGetVer(lTable, level);
	eventLevelWrite(level);

	// no timeTable exists so create it
	if (table == NULL) {
		eventTimeTableNewAlloc(level, type);
		table = TimeTableAlloc(type);

		LTableSetTable(lTable, level, table); 
		assert(table->type == type);
		TimeTableSet(table, addr, value, verCurrent, type);

	} else {
		// convert the table if needed
		if (type == TYPE_32BIT && table->type == TYPE_64BIT) {
			TimeTable* old = table;
			table = TimeTableConvert(table);
			TimeTableFree(old, lTable->isCompressed);
		}

		if (verOld == verCurrent) {
			assert(table != NULL);
			TimeTableSet(table, addr, value, verCurrent, type);

		} else {
			// exists but version is old so clean it and reuse
			TimeTableClean(table);
			TimeTableSet(table, addr, value, verCurrent, type);
		}
	}
	LTableSetVer(lTable, level, verCurrent);
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


/*
 * Garbage collection related logic
 */
static UInt64 nextGC = 1024;
static int gcPeriod = -1;

static void setGCPeriod(int time) {
	fprintf(stderr, "[kremlin] set GC period to %d\n", time);
	nextGC = time;
	gcPeriod = time;
	if (time == 0)
		nextGC = 0xFFFFFFFFFFFFFFFF;
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
	eventGC();
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

/*
 * LTable operations
 */

static inline LTable* LTableGet(Addr addr, Version* vArray) {
	SEntry* sEntry = STableGetSEntry(addr);
	SegTable* segTable = sEntry->segTable;
	assert(segTable != NULL);
	int segIndex = SegTableGetIndex(addr);
	LTable* lTable = segTable->entry[segIndex];
	if (lTable == NULL) {
		lTable = LTableAlloc();
		if (useCompression()) {
			int compressGain = CBufferAdd(lTable);
			eventCompression(compressGain);
		}
		segTable->entry[segIndex] = lTable;
		eventLTableAlloc();
	}
	
	if(useCompression() && lTable->isCompressed) {
		gcLevelUnknownSize(lTable,vArray);
		int gain = CBufferDecompress(lTable);
		eventCompression(gain);
	}


	return lTable;
}
static void check(Addr addr, Time* src, int size, int site) {
#ifndef NDEBUG
	int i;

	for (i=1; i<size; i++) {
		if (src[i-1] < src[i]) {
			fprintf(stderr, "site %d Addr %p size %d offset %d val=%ld %ld\n", 
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

/*
 * Fetch / Evict from TVCache to TVStorage
 */

void SkaduEvict(Time* tArray, Addr addr, int size, Version* vArray, int type) {
	if (addr == NULL)
		return;

	int i;
	//int startInvalid = getStartInvalidLevel(oldVersion, vArray, size);

	MSG(0, "\tTVCacheEvict 0x%llx, size=%d, effectiveSize=%d \n", addr, size, size);
		
	LTable* lTable = LTableGet(addr,vArray);
	for (i=0; i<size; i++) {
		eventEvict(i);
		if (tArray[i] == 0ULL) {
			break;
		}
		LTableSetTime(lTable, i, addr, vArray[i], tArray[i], type);
		MSG(0, "\t\toffset=%d, version=%d, value=%d\n", i, vArray[i], tArray[i]);
	}
	eventCacheEvict(size, size);

	//fprintf(stderr, "\tTVCacheEvict lTable=%llx, 0x%llx, size=%d, effectiveSize=%d \n", lTable, addr, size, startInvalid);
	if (useCompression())
		CBufferAccess(lTable);
	
	
	check(addr, tArray, size, 3);
}

void SkaduFetch(Addr addr, Index size, Version* vArray, Time* destAddr, int type) {
	MSG(0, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	//fprintf(stderr, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	LTable* lTable = LTableGet(addr, vArray);

	int i;
	for (i=0; i<size; i++) {
		destAddr[i] = LTableGetTime(lTable, i, addr, vArray[i], type);
	}

	if (useCompression())
		CBufferAccess(lTable);
}


/*
 * Actual load / store handlers without TVCache
 */

static Time tempArray[1000];

static Time* NoCacheGet(Addr addr, Index size, Version* vArray, int type) {
	LTable* lTable = LTableGet(addr, vArray);	
	Index i;
	for (i=0; i<size; i++) {
		tempArray[i] = LTableGetTime(lTable, i, addr, vArray[i], type);
	}

	if (useCompression())
		CBufferAccess(lTable);

	return tempArray;	
}

static void NoCacheSet(Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	LTable* lTable = LTableGet(addr, vArray);	
	assert(lTable != NULL);
	Index i;
	for (i=0; i<size; i++) {
		LTableSetTime(lTable, i, addr, vArray[i], tArray[i], type);
	}

	if (useCompression())
		CBufferAccess(lTable);
}


/*
 * Entry point functions from Kremlin
 */

static Time* _MShadowSkaduGet(Addr addr, Index size, Version* vArray, UInt32 width) {
	if (size < 1)
		return NULL;

	//int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	int type = TYPE_64BIT; 

	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "MShadowGet 0x%llx, size %d \n", tAddr, size);
	eventRead();
	if (KConfigUseSkaduCache() == FALSE) {
		return NoCacheGet(tAddr, size, vArray, type);

	} else {
		return TVCacheGet(tAddr, size, vArray, type);
	}
}

static void _MShadowSkaduSet(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	MSG(0, "MShadowSet 0x%llx, size %d [", addr, size);
	if (size < 1)
		return;

	if (getActiveTimeTableSize() >= nextGC) {
		gcStart(vArray, size);
		//nextGC = stat.nTimeTableActive + gcPeriod;
		nextGC += gcPeriod;
	}

	//int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	int type = TYPE_64BIT;


	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "]\n");
	eventWrite();
	if (KConfigUseSkaduCache() == FALSE) {
		NoCacheSet(tAddr, size, vArray, tArray, type);

	} else {
		TVCacheSet(tAddr, size, vArray, tArray, type);
	}
}

/*
 * Init / Deinit
 */

UInt MShadowInitSkadu() {
	int cacheSizeMB = KConfigGetSkaduCacheSize();
	fprintf(stderr,"[kremlin] MShadow Init with cache %d MB, TimeTableSize = %ld\n",
		cacheSizeMB, sizeof(TimeTable));

	int size = TimeTableEntrySize(TYPE_64BIT);
	MemPoolInit(1024, size * sizeof(Time));
	
	setGCPeriod(KConfigGetGCPeriod());
 
	STableInit();
	TVCacheInit(cacheSizeMB);

	CBufferInit(KConfigGetCBufferSize());
	MShadowGet = _MShadowSkaduGet;
	MShadowSet = _MShadowSkaduSet;
	setCompression();
	return 0;
}


UInt MShadowDeinitSkadu() {
	CBufferDeinit();
	MShadowStatPrint();
	STableDeinit();
	TVCacheDeinit();
	return 0;
}




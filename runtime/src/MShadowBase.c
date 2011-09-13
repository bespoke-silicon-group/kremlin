#include "defs.h"

#ifdef USE_MSHADOW_BASE

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

#define USE_CACHE

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)
#define DS_ALLOC_SIZE   100     // used for static data structures
#define MAX_SRC_TSA_VAL	6
#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

// Mask of the L1 table.
#define L1_MASK 0xfffff

// Size of the L1 table.
#define L1_SIZE (L1_MASK + 1)

// Bits to shift right before the mask
#define L1_SHIFT 12

// Mask of the L2 table.
#define L2_MASK 0x3ff

// The size of the L2 table. Since only word addressible, shift by 2
#define L2_SIZE (L2_MASK + 1)
#define L2_SHIFT 2

typedef struct _TimeTable {
	Time array[L2_SIZE];
} TimeTable;


typedef struct _SegEntry {
	TimeTable* tTable;
	TimeTable* vTable;
	int depth;
} SegEntry;


typedef struct _Segment {
	SegEntry entry[L1_SIZE];
} SegTable;


/*
 * MemStat
 */
typedef struct _MemStat {
	UInt64 nSTableEntry;
	UInt64 nSegTableAllocated;
	UInt64 nSegTableActive;
	UInt64 nSegTableActiveMax;

	UInt64 nTimeTableAllocated;
	UInt64 nTimeTableFreed;
	UInt64 nTimeTableActive;
	UInt64 nTimeTableActiveMax;

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

	double segTableSize = sizeof(SegTable) * stat.nSegTableAllocated / (1024.0 * 1024.0);

	int timeTableEach = sizeof(Time) * L2_SIZE * getMaxActiveLevel();
	double timeTableSize = timeTableEach * stat.nTimeTableActiveMax / (1024.0 * 1024.0 * 2);
	double versionTableSize = timeTableSize;
	fprintf(stderr, "SegTable size = %.2f MB\n", segTableSize); 
	fprintf(stderr, "TimeTable size = %.2f MB\n", timeTableSize); 
	fprintf(stderr, "VersionTable size = %.2f MB\n", versionTableSize);
	fprintf(stderr, "Total Mem size = %.2f MB\n", 
		segTableSize + timeTableSize + versionTableSize);
}


/*
 * TimeTable: simple array of Time with L2_SIZE elements
 *
 */ 


TimeTable* TimeTableAlloc(int depth) {
	stat.nTimeTableAllocated++;
	stat.nTimeTableActive++;
	if (stat.nTimeTableActive > stat.nTimeTableActiveMax)
		stat.nTimeTableActiveMax++;
	return (TimeTable*) calloc(sizeof(Time), L2_SIZE * depth);
}

void TimeTableFree(TimeTable* table) {
	stat.nTimeTableActive--;
	stat.nTimeTableFreed++;
	free(table);
}

static int TimeTableGetIndex(Addr addr, int depth) {
    int ret = ((UInt64)addr >> L2_SHIFT) & L2_MASK;
	assert(ret < L2_SIZE);
	return ret * depth;
}

static Time* TimeTableGetAddr(TimeTable* table, Addr addr, int depth) {
	int index = TimeTableGetIndex(addr, depth);
	return &(table->array[index]);
}


/*
 * SegTable:
 *
 */ 


SegTable* SegTableAlloc() {
	SegTable* ret = (SegTable*) calloc(sizeof(SegEntry), L1_SIZE);

	int i;
	for (i=0; i<L1_SIZE; i++) {
		ret->entry[i].depth = 32;
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
		if (table->entry[i].tTable != NULL) {
			TimeTableFree(table->entry[i].tTable);	
			TimeTableFree(table->entry[i].vTable);	
		}
	}
	stat.nSegTableActive--;
	free(table);
}

static int SegTableGetIndex(Addr addr) {
	return ((UInt64)addr >> L1_SHIFT) & L1_MASK;
}

static SegEntry* SegTableGetEntry(SegTable* segTable, Addr addr) {
	int index = SegTableGetIndex(addr);
	SegEntry* entry = &(segTable->entry[index]);
	return entry;
}

void SegTableSetTime(SegEntry* entry, Addr addr, Index size, Version* vArray, Time* tArray) {
	if (entry->tTable == NULL) {
		entry->tTable = TimeTableAlloc(entry->depth);
		entry->vTable = TimeTableAlloc(entry->depth);
	}

	// set version and time
	Time* tAddr = TimeTableGetAddr(entry->tTable, addr, entry->depth);
	memcpy(tAddr, tArray, sizeof(Time) * size);
	Time* vAddr = TimeTableGetAddr(entry->vTable, addr, entry->depth);
	int i;
	for (i=size-1; i>=0; i--) {
		if (vAddr[i] != vArray[i]) {
			//memcpy(vAddr, vArray, sizeof(Time) * size);
			vAddr[i] = vArray[i];
			
		} else {
			break;
		}
	}
}

Time* SegTableGetTime(SegEntry* entry, Addr addr, Index size, Version* vArray) {
	if (entry->tTable == NULL) {
		entry->tTable = TimeTableAlloc(entry->depth);
		entry->vTable = TimeTableAlloc(entry->depth);
	}

	Time* tAddr = TimeTableGetAddr(entry->tTable, addr, entry->depth);
	Time* vAddr = TimeTableGetAddr(entry->vTable, addr, entry->depth);
	int i;
	for (i=size-1; i>=0; i--) {
		Version oldVersion = (Version)vAddr[i];
		if (oldVersion != vArray[i]) {
			tAddr[i] = 0ULL;
			vAddr[i] = vArray[i];
		} else
			break;
	}
	return tAddr;
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

	for (i=0; i<sTable.writePtr; i++) {
		SegTableFree(sTable.entry[i].segTable);		
	}
}

SegTable* STableGetSegTable(Addr addr) {
	UInt32 highAddr = (UInt32)((UInt64)addr >> 32);

	// walk-through STable
	int i;
	for (i=0; i<sTable.writePtr; i++) {
		if (sTable.entry[i].addrHigh == highAddr) {
			//MSG(0, "STable Found an existing entry..\n");
			return sTable.entry[i].segTable;	
		}
	}

	// not found - create an entry
	MSG(0, "STable Creating a new Entry..\n");
	stat.nSTableEntry++;

	SegTable* ret = SegTableAlloc();
	sTable.entry[sTable.writePtr].addrHigh = highAddr;
	sTable.entry[sTable.writePtr].segTable = ret;
	sTable.writePtr++;
	return ret;
}


#define WORD_SHIFT		2

#define TABLE_SHIFT		10	

#define LINE_SHIFT	20
#define NUM_LINE		(1 << LINE_SHIFT)
#define NUM_LINE_MASK	(NUM_LINE - 1)

#define STATUS_VALID	1


typedef struct _L1Entry {
	UInt64 tag;
	SegEntry* segEntry;
	UInt32 status;

} L1Entry;

typedef struct _MShadowL1 {
	L1Entry entry[NUM_LINE];
} MShadowL1;

static MShadowL1	cache;

static inline UInt64 getTag(Addr addr) {
	int nShift = WORD_SHIFT + TABLE_SHIFT + LINE_SHIFT;
	UInt64 mask = ~((1 << nShift) - 1);
	return (UInt64)addr & mask;
}

static inline int getLineIndex(Addr addr) {
	int nShift = WORD_SHIFT + TABLE_SHIFT;
	int ret = (((UInt64)addr) >> nShift ) & NUM_LINE_MASK;
	return ret;
}

static inline void setValid(L1Entry* entry) {
	entry->status |= STATUS_VALID;
}

static inline Bool isValid(L1Entry* entry) {
	return entry->status & STATUS_VALID;
}

static inline void setTag(L1Entry* entry, UInt64 tag) {
	entry->tag = tag;
}

static inline Bool isHit(L1Entry* entry, Addr addr) {
	return isValid(entry) && (entry->tag == getTag(addr));
}

static inline L1Entry* getEntry(Addr addr) {
	int index = getLineIndex(addr);
	L1Entry* entry = &(cache.entry[index]);

	if (isHit(entry, addr) == FALSE) {
		// bring the cache line
		SegTable* segTable = STableGetSegTable(addr);
		SegEntry* segEntry = SegTableGetEntry(segTable, addr);
		entry->segEntry = segEntry;
		setValid(entry);
		setTag(entry, getTag(addr));
	}
	return entry;
}


UInt MShadowInit(int a, int b) {
	fprintf(stderr, "[kremlin] MShadow Base Init\n");
	STableInit();
	MemAllocInit(sizeof(TimeTable));
}


UInt MShadowDeinit() {
	printMemStat();
	STableDeinit();
	MemAllocDeinit();
}

Time* MShadowGet(Addr addr, Index size, Version* vArray) {
	MSG(0, "MShadowGet 0x%llx, size %d\n", addr, size);
	SegEntry* segEntry = NULL;
#ifdef USE_CACHE
	L1Entry* entry = getEntry(addr);
	assert(entry->segEntry != NULL);
	segEntry = entry->segEntry;
#else	
	SegTable* segTable = STableGetSegTable(addr);
	segEntry = SegTableGetEntry(segTable, addr);
#endif
	return SegTableGetTime(segEntry, addr, size, vArray);
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray) {
	SegEntry* segEntry = NULL;
#ifdef USE_CACHE
	L1Entry* entry = getEntry(addr);
	assert(entry->segEntry != NULL);
	segEntry = entry->segEntry;
#else	
	SegTable* segTable = STableGetSegTable(addr);
	segEntry = SegTableGetEntry(segTable, addr);
	assert(entry->segEntry != NULL);
#endif

	SegTableSetTime(segEntry, addr, size, vArray, tArray);
}


#endif

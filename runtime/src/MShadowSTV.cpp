#include "kremlin.h"
#include "config.h"
#include "MShadowSTV.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "debug.h"
//#include "MShadow.h"

#define DEBUGLEVEL 3

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

// Mask of the L1 table.
#define L1_MASK 0xffff

// Size of the L1 table.
#define L1_SIZE (L1_MASK + 1)

// Bits to shift right before the mask
#define L1_SHIFT 16

// Mask of the L2 table.
#define L2_MASK 0x3fff

// The size of the L2 table. Since only word addressible, shift by 2
#define L2_SIZE (L2_MASK + 1)
#define L2_SHIFT 2

#define TYPE_64BIT	0
#define TYPE_32BIT	1

typedef struct _SegEntry {
	Time* tTable;
	Version* versions;
	int type;
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

	UInt64 nTimeTableAllocated[2];
	UInt64 nTimeTableFreed[2];
	UInt64 nTimeTableActive;
	UInt64 nTimeTableActiveMax;
	
	UInt64 nTimeTableConverted;

} MemStat;

static MemStat stat;

static void printMemStat() {
	fprintf(stderr, "nSTableEntry = %llu\n\n", stat.nSTableEntry);

	fprintf(stderr, "nSegTableAllocated = %llu\n", stat.nSegTableAllocated);
	fprintf(stderr, "nSegTableActive = %llu\n", stat.nSegTableActive);
	fprintf(stderr, "nSegTableActiveMax = %llu\n\n", stat.nSegTableActiveMax);

	fprintf(stderr, "nTimeTableAllocated = %llu, %llu\n", 
		stat.nTimeTableAllocated[0], stat.nTimeTableAllocated[1]);
	fprintf(stderr, "nTimeTableFreed = %llu, %lld\n", 
		stat.nTimeTableFreed[0], stat.nTimeTableFreed[1]);
	fprintf(stderr, "nTimeTableActive = %llu\n", stat.nTimeTableActive);
	fprintf(stderr, "nTimeTableActiveMax = %llu\n\n", stat.nTimeTableActiveMax);
	fprintf(stderr, "nTimeTableConverted = %llu\n\n", stat.nTimeTableConverted);

	double segTableSize = sizeof(SegTable) * stat.nSegTableAllocated / (1024.0 * 1024.0);

	//int timeTableEach64 = sizeof(Time) * (L2_SIZE/2) * getMaxActiveLevel();
	int timeTableEach64 = sizeof(Time) * (L2_SIZE/2) * getMaxActiveLevel();
	UInt64 nTable64 = stat.nTimeTableAllocated[0] - stat.nTimeTableFreed[0];
	UInt64 nTable32 = stat.nTimeTableAllocated[1] - stat.nTimeTableFreed[1];
	double timeTableSize = timeTableEach64 * (nTable64 + 2*nTable32) / (1024.0 * 1024.0);

	int verTableEach64 = sizeof(Version) * (L2_SIZE/2);
	double versionTableSize = verTableEach64 * (nTable64 + 2*nTable32) / (1024.0 * 1024.0);

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

static int getTimeTableEntrySize(int type) {
	int nEntry = L2_SIZE;
	if (type == TYPE_64BIT)
		nEntry = L2_SIZE / 2;

	return nEntry;
}

static Time* TimeTableAlloc(int type, int depth) {
	stat.nTimeTableAllocated[type]++;
	stat.nTimeTableActive++;
	if (stat.nTimeTableActive > stat.nTimeTableActiveMax)
		stat.nTimeTableActiveMax++;


	//fprintf(stderr, "TAlloc: type = %d, depth = %d\n", type, depth);
	int nEntry = getTimeTableEntrySize(type);
	Time* ret = (Time*) calloc(sizeof(Time) * nEntry * depth, 1);
	MSG(DEBUGLEVEL, "TTableAlloc: 0x%llx\n", ret);
	return ret;
}

static void TimeTableFree(Time* table, int type) {
	stat.nTimeTableActive--;
	stat.nTimeTableFreed[type]++;
	free(table);
}

static inline int SegTableEntryOffset(Addr addr, int type) {
    int ret = ((UInt64)addr >> L2_SHIFT) & L2_MASK;
	if (type == TYPE_64BIT)
		ret = ret / 2;
	return ret;
}

static int TimeTableGetIndex(Addr addr, int depth, int type) {
	int offset = SegTableEntryOffset(addr, type);
    return offset * depth;
}


static inline Time* TimeTableGetAddr(SegEntry* entry, Addr addr) {
	int index = TimeTableGetIndex(addr, entry->depth, entry->type);
	Time* ret = &(entry->tTable[index]);
	MSG(DEBUGLEVEL, "TTableGetAddr : 0x%llx\n", ret);
	return ret;
}

static inline Version* VersionGetAddr(SegEntry* entry, Addr addr) {
	int index = SegTableEntryOffset(addr, entry->type);
	return &(entry->versions[index]);
}

/*
 * SegTable:
 *
 */ 


static SegTable* SegTableAlloc() {
	SegTable* ret = (SegTable*) calloc(sizeof(SegEntry), L1_SIZE);

	int i;
	for (i=0; i<L1_SIZE; i++) {
		ret->entry[i].depth = kremlin_config.getNumProfiledLevels();
	}

	stat.nSegTableAllocated++;
	stat.nSegTableActive++;
	if (stat.nSegTableActive > stat.nSegTableActiveMax)
		stat.nSegTableActiveMax++;
	return ret;	
}

static void SegTableFree(SegTable* table) {
	int i;
	for (i=0; i<L1_SIZE; i++) {
		if (table->entry[i].tTable != NULL) {
			TimeTableFree(table->entry[i].tTable, table->entry[i].type);	
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

static void TagValidate(Time* tAddr, Version* vAddr, Version* vArray, int size) {
	int i;
	int startInvalid = 0;
	Version oldVersion = *vAddr;

	for (i=size-1; i>=0; i--) {
		if (vArray[i] <= oldVersion) {
			startInvalid = i+1;
			break;
		}
	}

	if (startInvalid < size) {
		bzero(tAddr + startInvalid, sizeof(Time) * (size - startInvalid));
	}
}

static inline Version* VersionAlloc(int type) {
	return (Version*)calloc(sizeof(Version), getTimeTableEntrySize(type));
}

static inline void VersionFree(Version* version) {
	free(version);
}

void SegEntryConvertTo32bit(SegEntry* entry) {
	assert(entry->type == TYPE_64BIT);
	stat.nTimeTableConverted++;
	int depth = entry->depth;
	Time* oldTime = entry->tTable;
	Time* newTime = TimeTableAlloc(TYPE_32BIT, depth);
	Version* oldVersion = entry->versions;
	Version* newVersion = VersionAlloc(TYPE_32BIT);

	int nEntry = getTimeTableEntrySize(TYPE_64BIT);
	int i;
	
	for (i=0; i<nEntry; i++) {
		memcpy(newTime + depth*2*i, oldTime + depth*i, sizeof(Time) * depth); 		
		memcpy(newTime + depth*2*i + 1, oldTime + depth*i, sizeof(Time) * depth); 		
		newVersion[i*2] = oldVersion[i];
		newVersion[i*2 + 1] = oldVersion[i];
	}

	TimeTableFree(entry->tTable, entry->type);
	VersionFree(entry->versions);
	entry->tTable = newTime;
	entry->versions = newVersion;
	entry->type = TYPE_32BIT;
}

void SegEntryExpandDepth(SegEntry* entry, int newDepth) {
	int oldDepth = entry->depth;
	Time* ret = TimeTableAlloc(entry->type, newDepth);
	int nEntry = getTimeTableEntrySize(TYPE_64BIT);
	int i;

	for (i=0; i<nEntry; i++) {
		memcpy(ret + newDepth*i, entry->tTable + oldDepth*i, sizeof(Time) * oldDepth); 		
	}
	
	TimeTableFree(entry->tTable, entry->type);
	entry->tTable = ret;
	entry->depth = newDepth;
	
}

static void SegEntryCheckConversion(SegEntry* entry, Version* vArray, int size, int type) {
	// check if it requires type conversion
	if (type == TYPE_32BIT && entry->type == TYPE_64BIT) {
		SegEntryConvertTo32bit(entry);
	}

	// check if it requires resizing
	if (entry->depth <= size) {
		SegEntryExpandDepth(entry, entry->depth * 2);
	}
}

static void SegTableSetTime(SegEntry* entry, Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	if (entry->tTable == NULL) {
		entry->tTable = TimeTableAlloc(type, entry->depth);
		entry->versions = VersionAlloc(type);

	} else {
		SegEntryCheckConversion(entry, vArray, size, type);
	}

	assert(entry->depth > size);
	Version* vAddr = VersionGetAddr(entry, addr);	
	Time* tAddr = TimeTableGetAddr(entry, addr);
	memcpy(tAddr, tArray, sizeof(Time) * size);
	*vAddr = vArray[size-1];
}

static Time* SegTableGetTime(SegEntry* entry, Addr addr, Index size, Version* vArray, int type) {
	if (entry->tTable == NULL) {
		entry->tTable = TimeTableAlloc(type, entry->depth);
		entry->versions = VersionAlloc(type);

	} else {
		SegEntryCheckConversion(entry, vArray, size, type);
	}

	assert(entry->depth > size);
	Time* tAddr = TimeTableGetAddr(entry, addr);
	Version* vAddr = VersionGetAddr(entry, addr);	
	TagValidate(tAddr, vAddr, vArray, size);
	*vAddr = vArray[size-1];
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

static void STableInit() {
	sTable.writePtr = 0;
}

static void STableDeinit() {
	int i;

	for (i=0; i<sTable.writePtr; i++) {
		SegTableFree(sTable.entry[i].segTable);		
	}
}

static SegTable* STableGetSegTable(Addr addr) {
	UInt32 highAddr = (UInt32)((UInt64)addr >> 32);

	// walk-through STable
	int i;
	for (i=0; i<sTable.writePtr; i++) {
		if (sTable.entry[i].addrHigh == highAddr) {
			//MSG(1, "STable Found an existing entry..\n");
			return sTable.entry[i].segTable;	
		}
	}

	// not found - create an entry
	MSG(DEBUGLEVEL, "STable Creating a new Entry..\n");
	stat.nSTableEntry++;

	SegTable* ret = SegTableAlloc();
	sTable.entry[sTable.writePtr].addrHigh = highAddr;
	sTable.entry[sTable.writePtr].segTable = ret;
	sTable.writePtr++;
	return ret;
}


Time* MShadowSTV::get(Addr addr, Index size, Version* vArray, UInt32 width) {
	MSG(DEBUGLEVEL, "MShadowGet 0x%llx, size %d\n", addr, size);

	if (size < 1)
		return NULL;

	SegEntry* segEntry = NULL;
	int type = TYPE_32BIT;
	if (width  > 4)
		type = TYPE_64BIT;

	SegTable* segTable = STableGetSegTable(addr);
	segEntry = SegTableGetEntry(segTable, addr);
	return SegTableGetTime(segEntry, addr, size, vArray, type);
}

void MShadowSTV::set(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	MSG(1, "MShadowSet 0x%llx, size %d\n", addr, size);

	if (size < 1)
		return;

	SegEntry* segEntry = NULL;
	int type = TYPE_32BIT;
	if (width  > 4)
		type = TYPE_64BIT;

	SegTable* segTable = STableGetSegTable(addr);
	segEntry = SegTableGetEntry(segTable, addr);
	//assert(entry->segEntry != NULL);
	SegTableSetTime(segEntry, addr, size, vArray, tArray, type);
}

void MShadowSTV::init() {
	fprintf(stderr, "[kremlin] MShadow STV Init\n");
	STableInit();
	//MemAllocInit(sizeof(TimeTable));
}

void MShadowSTV::deinit() {
	printMemStat();
	STableDeinit();
	//MemAllocDeinit();
}

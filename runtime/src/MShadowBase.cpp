#include "kremlin.h"
#include "config.h"
#include "MShadowBase.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

#define L1_MASK 0xffff 			// Mask of the L1 table.
#define L1_SIZE (L1_MASK + 1)   // Size of the L1 table.
#define L1_SHIFT 16				// Bits to shift right before the mask
#define L2_MASK 0x3fff			// Mask of the L2 table.
#define L2_SIZE (L2_MASK + 1)	// Size of the L2 table
#define L2_SHIFT 2				// 32-bit word offset

// Access types
#define TYPE_64BIT	0
#define TYPE_32BIT	1

/* 
 * Segment Entry: track a 64KB chunk
 */
typedef struct _SegEntry {
	Time* tTable;
	Version* versions;
	int type;
	int depth;
} SegEntry;


/*
 * SegTable: track a 4GB chunk
 */
typedef struct _Segment {
	SegEntry entry[L1_SIZE];
} SegTable;


/*
 * MemStat: track and print statistics
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

	UInt64 reqRead[64];
	UInt64 reqWrite[64];

} MemStat;

static MemStat stat;

static void printMemStat() {
	fprintf(stderr, "nSTableEntry = %llu\n\n", stat.nSTableEntry);

	fprintf(stderr, "nSegTableAllocated = %llu\n", stat.nSegTableAllocated);
	fprintf(stderr, "nSegTableActive = %llu\n", stat.nSegTableActive);
	fprintf(stderr, "nSegTableActiveMax = %llu\n\n", stat.nSegTableActiveMax);

	fprintf(stderr, "nTimeTableAllocated = %llu, %llu\n", 
		stat.nTimeTableAllocated[0], stat.nTimeTableAllocated[1]);
	fprintf(stderr, "nTimeTableFreed = %llu, %llu\n", 
		stat.nTimeTableFreed[0], stat.nTimeTableFreed[1]);
	fprintf(stderr, "nTimeTableActive = %llu\n", stat.nTimeTableActive);
	fprintf(stderr, "nTimeTableActiveMax = %llu\n\n", stat.nTimeTableActiveMax);
	fprintf(stderr, "nTimeTableConverted = %llu\n\n", stat.nTimeTableConverted);

	double segTableSize = sizeof(SegTable) * stat.nSegTableAllocated / (1024.0 * 1024.0);

	int timeTableEach64 = sizeof(Time) * (L2_SIZE/2) * getMaxActiveLevel();
	UInt64 nTable64 = stat.nTimeTableAllocated[0] - stat.nTimeTableFreed[0];
	UInt64 nTable32 = stat.nTimeTableAllocated[1] - stat.nTimeTableFreed[1];
	double timeTableSize = timeTableEach64 * (nTable64 + 2*nTable32) / (1024.0 * 1024.0) / 2;
	double versionTableSize = timeTableSize;

	int i;
	for (i=0; i < kremlin_config.getNumProfiledLevels(); i++) {
		fprintf(stderr, "Level [%2d] req Read / Write = %lld, %lld\n",
			i, stat.reqRead[i-1], stat.reqWrite[i-1]);
	}

	fprintf(stderr, "SegTable size = %.2f MB\n", segTableSize); 
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

	int nEntry = getTimeTableEntrySize(type);
	return (Time*) malloc(sizeof(Time) * nEntry * depth);
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
	return &(entry->tTable[index]);
}

static inline Version* VersionGetAddr(SegEntry* entry, Addr addr) {
	int index = TimeTableGetIndex(addr, entry->depth, entry->type);
	return &(entry->versions[index]);
}


/*
 * SegTable related functions
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

	for (i=size-1; i>=0; i--) {
		if (vArray[i] == vAddr[i]) {
			startInvalid = i+1;
			break;
		}
	}

	if (startInvalid < size) {
		bzero(tAddr + startInvalid, sizeof(Time) * (size - startInvalid));
		memcpy(&vAddr[startInvalid], &vArray[startInvalid], sizeof(Version) * (size - startInvalid));
	}
}

static inline Version* VersionTableAlloc(int type, int depth) {
	return TimeTableAlloc(type, depth);
}

static inline void VersionTableFree(Version* version, int type) {
	TimeTableFree(version, type);
}

static Time* convertTable(Time* table, int depth) {
	stat.nTimeTableConverted++;
	Time* ret = TimeTableAlloc(TYPE_32BIT, depth);
	int nEntry = getTimeTableEntrySize(TYPE_64BIT);
	int i;
	
	for (i=0; i<nEntry; i++) {
		memcpy(ret + depth*2*i, table + depth*i, sizeof(Time) * depth); 		
		memcpy(ret + depth*2*i + 1, table + depth*i, sizeof(Time) * depth); 		
	}
	return ret;
}

static Version* convertVersion(Version* src, int depth) {
	Time* ret = VersionTableAlloc(TYPE_32BIT, depth);
	int nEntry = getTimeTableEntrySize(TYPE_64BIT);
	int i;
	
	for (i=0; i<nEntry; i++) {
		memcpy(ret + depth*2*i, src + depth*i, sizeof(Version) * depth); 		
		memcpy(ret + depth*2*i + 1, src + depth*i, sizeof(Version) * depth); 		
	}
	return ret;

}



static void checkRefresh(SegEntry* entry, Version* vArray, int size, int type) {
	if (type == TYPE_32BIT && entry->type == TYPE_64BIT) {
		// convert time table
		Time* converted = convertTable(entry->tTable, entry->depth);
		TimeTableFree(entry->tTable, entry->type);
		entry->tTable = converted;

		// convert version table
		Version* version = convertVersion(entry->versions, entry->depth);
		VersionTableFree(entry->versions, entry->type);
		entry->versions = version;

		entry->type = TYPE_32BIT;
	}
}

static void SegTableSetTime(SegEntry* entry, Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	if (entry->tTable == NULL) {
		entry->tTable = TimeTableAlloc(type, entry->depth);
		entry->versions = VersionTableAlloc(type, entry->depth);

	} else {
		checkRefresh(entry, vArray, size, type);
	}

	Time* tAddr = TimeTableGetAddr(entry, addr);
	memcpy(tAddr, tArray, sizeof(Time) * size);

	Version* vAddr = VersionGetAddr(entry, addr);	
	memcpy(vAddr, vArray, sizeof(Version) * size);
}

static Time* SegTableGetTime(SegEntry* entry, Addr addr, Index size, Version* vArray, int type) {
	if (entry->tTable == NULL) {
		entry->tTable = TimeTableAlloc(type, entry->depth);
		entry->versions = VersionTableAlloc(type, entry->depth);

	} else {
		checkRefresh(entry, vArray, size, type);
	}

	Time* tAddr = TimeTableGetAddr(entry, addr);
	Version* vAddr = VersionGetAddr(entry, addr);	
	TagValidate(tAddr, vAddr, vArray, size);
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
	UInt32 	addrHigh;	// upper 32 bits in 64bit addr
	SegTable* segTable;
} SEntry;

#define STABLE_SIZE		32		// 128GB addr space will be large enough

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
			MSG(1, "STable Found an existing entry..\n");
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



Time* MShadowBase::get(Addr addr, Index size, Version* vArray, UInt32 width) {
	MSG(0, "MShadowGet 0x%llx, size %d\n", addr, size);

	if (size < 1)
		return NULL;

	int type = TYPE_32BIT;
	if (width  > 4)
		type = TYPE_64BIT;

	SegEntry* segEntry = NULL;
	SegTable* segTable = STableGetSegTable(addr);
	segEntry = SegTableGetEntry(segTable, addr);
	stat.reqRead[size]++;
	return SegTableGetTime(segEntry, addr, size, vArray, type);
}

void MShadowBase::set(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	if (size < 1)
		return;

	int type = TYPE_32BIT;
	if (width  > 4)
		type = TYPE_64BIT;

	SegEntry* segEntry = NULL;
	SegTable* segTable = STableGetSegTable(addr);
	segEntry = SegTableGetEntry(segTable, addr);
	SegTableSetTime(segEntry, addr, size, vArray, tArray, type);
	stat.reqWrite[size]++;
}


void MShadowBase::init() {
	fprintf(stderr, "[kremlin] MShadow Base Init\n");
	STableInit();
}


void MShadowBase::deinit() {
	printMemStat();
	STableDeinit();
}

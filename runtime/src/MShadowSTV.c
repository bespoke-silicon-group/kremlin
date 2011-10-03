#include "defs.h"

#if TYPE_MSHADOW == MSHADOW_STV

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "debug.h"
#include "MShadow.h"

//#define USE_CACHE

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
	//Time tTable[L2_SIZE][32];
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

void printMemStat() {
	fprintf(stderr, "nSTableEntry = %d\n\n", stat.nSTableEntry);

	fprintf(stderr, "nSegTableAllocated = %lld\n", stat.nSegTableAllocated);
	fprintf(stderr, "nSegTableActive = %d\n", stat.nSegTableActive);
	fprintf(stderr, "nSegTableActiveMax = %d\n\n", stat.nSegTableActiveMax);

	fprintf(stderr, "nTimeTableAllocated = %lld, %lld\n", 
		stat.nTimeTableAllocated[0], stat.nTimeTableAllocated[1]);
	fprintf(stderr, "nTimeTableFreed = %lld, %lld\n", 
		stat.nTimeTableFreed[0], stat.nTimeTableFreed[1]);
	fprintf(stderr, "nTimeTableActive = %lld\n", stat.nTimeTableActive);
	fprintf(stderr, "nTimeTableActiveMax = %lld\n\n", stat.nTimeTableActiveMax);
	fprintf(stderr, "nTimeTableConverted = %lld\n\n", stat.nTimeTableConverted);

	double segTableSize = sizeof(SegTable) * stat.nSegTableAllocated / (1024.0 * 1024.0);

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

Time* TimeTableAlloc(int type, int depth) {
	stat.nTimeTableAllocated[type]++;
	stat.nTimeTableActive++;
	if (stat.nTimeTableActive > stat.nTimeTableActiveMax)
		stat.nTimeTableActiveMax++;


	//fprintf(stderr, "TAlloc: type = %d, depth = %d\n", type, depth);
	int nEntry = getTimeTableEntrySize(type);
	return (Time*) malloc(sizeof(Time) * nEntry * depth);
}

void TimeTableFree(Time* table, int type) {
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
	int index = SegTableEntryOffset(addr, entry->type);
	return &(entry->versions[index]);
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

#if 0
static void refreshTimeTable(SegEntry* entry, Version* vArray, int size) {
	int i;
	int startInvalid = 0;
	int nEntry = getTimeTableEntrySize(entry->type);

	Version oldVersion = entry->version;
	for (i=size-1; i>=0; i--) {
		if (vArray[i] <= oldVersion) {
			startInvalid = i+1;
			break;
		}
	}

	// clear level[startInvalid:size-1] to 0ULL
	if (startInvalid < size) {
		for (i=0; i<nEntry; i++) {
			Time* addr = &(entry->tTable[i * entry->depth]);
			bzero(addr, sizeof(Time) * (size - startInvalid));
		}
	}
}
#endif

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
	return calloc(sizeof(Version), getTimeTableEntrySize(type));
}

static inline void VersionFree(Version* version) {
	free(version);
}

Time* convertTable(Time* table, int depth) {
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

Version* convertVersion(Version* src) {
	int i;
	Version* ret = VersionAlloc(TYPE_32BIT);
	for (i=0; i<getTimeTableEntrySize(TYPE_64BIT); i++) {
		ret[i*2] = src[i];
		ret[i*2 + 1] = src[i];
	}
	return ret;
}



static void checkRefresh(SegEntry* entry, Version* vArray, int size, int type) {
/*
	Version oldVersion = entry->version;
	fprintf(stderr, "[%d, %d]\t", oldVersion, vArray[size-1]);
	if (oldVersion != vArray[size-1]) {
		refreshTimeTable(entry, vArray, size);
		entry->version = vArray[size-1];
		fprintf(stderr, "?");
	} else {
		fprintf(stderr, "!");
	}
*/

	if (type == TYPE_32BIT && entry->type == TYPE_64BIT) {
		Time* converted = convertTable(entry->tTable, entry->depth);
		TimeTableFree(entry->tTable, entry->type);
		entry->tTable = converted;

		Version* version = convertVersion(entry->versions);
		VersionFree(entry->versions);
		entry->versions = version;

		entry->type = TYPE_32BIT;
	}
	
	
}

void SegTableSetTime(SegEntry* entry, Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	if (entry->tTable == NULL) {
		entry->tTable = TimeTableAlloc(type, entry->depth);
		entry->versions = VersionAlloc(type);

	} else {
		checkRefresh(entry, vArray, size, type);
	}

	Version* vAddr = VersionGetAddr(entry, addr);	
	Time* tAddr = TimeTableGetAddr(entry, addr);
	memcpy(tAddr, tArray, sizeof(Time) * size);
	*vAddr = vArray[size-1];
}

Time* SegTableGetTime(SegEntry* entry, Addr addr, Index size, Version* vArray, int type) {
	if (entry->tTable == NULL) {
		entry->tTable = TimeTableAlloc(type, entry->depth);
		entry->versions = VersionAlloc(type);

	} else {
		checkRefresh(entry, vArray, size, type);
	}

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


#ifdef USE_CACHE

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
#endif


UInt MShadowInit(int a, int b) {
	fprintf(stderr, "[kremlin] MShadow Base Init\n");
	STableInit();
	//MemAllocInit(sizeof(TimeTable));
}


UInt MShadowDeinit() {
	printMemStat();
	STableDeinit();
	//MemAllocDeinit();
}

Time* MShadowGet(Addr addr, Index size, Version* vArray, UInt32 width) {
	MSG(0, "MShadowGet 0x%llx, size %d\n", addr, size);

	SegEntry* segEntry = NULL;
	int type = TYPE_32BIT;
	if (width  > 4)
		type = TYPE_64BIT;

#ifdef USE_CACHE
	L1Entry* entry = getEntry(addr);
	assert(entry->segEntry != NULL);
	segEntry = entry->segEntry;
#else	
	SegTable* segTable = STableGetSegTable(addr);
	segEntry = SegTableGetEntry(segTable, addr);
#endif
	return SegTableGetTime(segEntry, addr, size, vArray, type);
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	SegEntry* segEntry = NULL;
	int type = TYPE_32BIT;
	if (width  > 4)
		type = TYPE_64BIT;

#ifdef USE_CACHE
	L1Entry* entry = getEntry(addr);
	assert(entry->segEntry != NULL);
	segEntry = entry->segEntry;
#else	
	SegTable* segTable = STableGetSegTable(addr);
	segEntry = SegTableGetEntry(segTable, addr);
	//assert(entry->segEntry != NULL);
#endif
	SegTableSetTime(segEntry, addr, size, vArray, tArray, type);
}


#endif

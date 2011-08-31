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

#define USE_VERSION_TABLE

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
	return NULL;
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

UInt MShadowInit() {
	assert(0);
	//GTableCreate(&gTable);	
	STableInit();
	MemAllocInit(sizeof(TimeTable));
	MShadowL1Init();
}


UInt MShadowDeinit() {
	//GTableDelete(&gTable);
	printMemStat();
	STableDeinit();
	MemAllocDeinit();
	MShadowL1Deinit();
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

#endif

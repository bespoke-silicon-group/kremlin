#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "defs.h"
#include "debug.h"
#include "hash_map.h"
#include "CRegion.h"
#include "Vector.h"
#include "MShadow.h"

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)
#define DS_ALLOC_SIZE   100     // used for static data structures
#define MAX_SRC_TSA_VAL	6
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

/*
 * MemStat
 */
typedef struct _MemStat {
	int nSTableEntry;

	int nSegTableAllocated;
	int nSegTableActive;
	int nSegTableActiveMax;

	int nTimeTableAllocated;
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
	fprintf(stderr, "nTimeTableActive = %d\n", stat.nTimeTableActive);
	fprintf(stderr, "nTimeTableActiveMax = %d\n\n", stat.nTimeTableActiveMax);
}

/*
 * Struct definitions
 */
typedef struct _GTEntry {
    unsigned short used; // number of entries that are in use
    unsigned short usedLine; // number of entries that are in use
    TEntry* array[L2_SIZE];
    TEntry* lineArray[L2_SIZE >> CACHE_LINE_POWER_2];
} GTEntry;

/*  
    GlobalTable:
        global table is a hashtable with lower address as its primary key.
*/
typedef struct _GTable {
    GTEntry* array[L1_SIZE];
} GTable;

static GTable* gTable;






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
	return (TimeTable*) calloc(sizeof(Time), L2_SIZE);
}

void TimeTableFree(TimeTable* table) {
	stat.nTimeTableActive--;
	free(table);
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
	table->array[index] = time;
}


/*
 * SegTable:
 *
 */ 

typedef struct _SegEntry {
	TimeTable* table;
	Version version;
} SegEntry;


typedef struct _Segment {
	SegEntry entry[L1_SIZE];
} SegTable;



SegTable* SegTableAlloc() {
	SegTable* ret = (SegTable*) calloc(sizeof(SegEntry), L1_SIZE);
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
		}
	}
	stat.nSegTableActive--;
	free(table);
}

static int SegTableGetIndex(Addr addr) {
	return ((UInt64)addr >> L1_SHIFT) & L1_MASK;
}


TimeTable* SegTableGetTimeTable(SegTable* segTable, Addr addr, Version version) {
	TimeTable* ret = NULL;
	int index = SegTableGetIndex(addr);
	SegEntry* entry = &(segTable->entry[index]);
	MSG(2, "SegTableGetTimeTable: addr = 0x%llx, version [prev, current] =[%d, %d]\n", 
		addr, entry->version, version);

	if (entry->table == NULL || entry->version != version) {
		ret = TimeTableAlloc();
		if (entry->table != NULL) {
			MSG(0, "\tFree Prev TimeTable with version %d\n", entry->version);
			TimeTableFree(entry->table);
		}
		entry->table = ret;
		entry->version = version;
	}

	return entry->table;
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

static Time getTime(Addr addr, Index index, Version version) {
	ITable* iTable = STableGetITable(addr);
	assert(iTable != NULL);
	SegTable* segTable = ITableGetSegTable(iTable, index);
	assert(segTable != NULL);
	TimeTable* tTable = SegTableGetTimeTable(segTable, addr, version);
	assert(tTable != NULL);
	return TimeTableGet(tTable, addr);	
}

static void setTime(Addr addr, Index index, Version version, Time time) {
	ITable* iTable = STableGetITable(addr);
	assert(iTable != NULL);
	SegTable* segTable = ITableGetSegTable(iTable, index);
	assert(segTable != NULL);
	TimeTable* tTable = SegTableGetTimeTable(segTable, addr, version);
	assert(tTable != NULL);
	TimeTableSet(tTable, addr, time);	
}

// TODO: reference count pages properly and free them when no longer used.
/*
 * Prototypes
 */
static int GTableCreate(GTable** t);
static int GTableDelete(GTable** t);
static TEntry* GTableGetTEntry(GTable* t, Addr addr);
static int GTableDeleteTEntry(GTable* t, Addr addr);

static int GTEntryCreate(GTEntry** e);
static int GTEntryDelete(GTEntry** e);
static int GTEntryDeleteTEntry(GTEntry*, Addr addr);
static TEntry** GTEntryGet(GTEntry* e, Addr addr);
static GTEntry** GTableGetGTEntry(GTable* t, Addr addr);
static UInt64 GTableIndex(Addr addr);
static UInt64 GTEntryIndex(Addr addr);


static int GTEntryCreate(GTEntry** e)
{
    if(!(*e = (GTEntry*)calloc(sizeof(GTEntry), 1)))
    {
        assert(0 && "calloc returned NULL in GTEntryCreate");
        return FALSE;
    }
    return TRUE;
}

static int GTEntryDelete(GTEntry** e)
{
    int i;
    for(i = 0; i < L2_SIZE; i++)
    {
        TEntry* tEntry = (*e)->array[i];
        if(tEntry)
            TEntryFree(tEntry);
    }

    free(*e);
    *e = NULL;
}

static int GTEntryDeleteTEntry(GTEntry* e, Addr addr)
{
    TEntry** tEntry = GTEntryGet(e, addr);
    TEntryFree(*tEntry);
    *tEntry = NULL;

    e->used--;
}

static TEntry** GTEntryGet(GTEntry* e, Addr addr)
{
    TEntry** tEntry = e->array + GTEntryIndex(addr);
    if(!*tEntry)
    {
        *tEntry = TEntryAlloc(getIndexSize());
        e->used++;
		e->usedLine += 1;
		// _tEntryGlobalCnt++;
    }
    return tEntry;
}

static UInt64 GTEntryIndex(Addr addr)
{
    return ((UInt64)addr >> L2_SHIFT) & L2_MASK;
}

static int GTableCreate(GTable** t)
{
    if(!(*t = (GTable*)calloc(1, sizeof(GTable))))
    {
        assert(0 && "calloc failed");
        return FALSE;
    }
    return TRUE;
}

static int GTableDelete(GTable** t) {
	int i, j;
	for (i = 0; i < L1_SIZE; i++) {
		if ((*t)->array[i] != NULL) {
            GTEntryDelete((*t)->array + i);
		}
	}
	free(*t);
    *t = NULL;
    return TRUE;
}

// get TEntry for address addr in GTable t
// FIXME: 64bit address?
static TEntry* GTableGetTEntry(GTable* t, Addr addr) 
{
#ifndef WORK_ONLY
    GTEntry** entry = GTableGetGTEntry(t, addr);
    return *GTEntryGet(*entry, addr);
#else
	return (TEntry*)1;
#endif
}

// get GTEntry in GTable t at address addr
static GTEntry** GTableGetGTEntry(GTable* t, Addr addr)
{
    UInt32 index = GTableIndex(addr);
	GTEntry** entry = t->array + index;
	if(*entry == NULL)
		GTEntryCreate(entry);
    return entry;
}

static int GTableDeleteTEntry(GTable* t, Addr addr)
{
    GTEntry** entry = GTableGetGTEntry(t, addr);
    GTEntryDeleteTEntry(*entry, addr);
    return TRUE;
}

static UInt64 GTableIndex(Addr addr)
{
	return ((UInt64)addr >> L1_SHIFT) & L1_MASK;
}


#ifdef EXTRA_STATS
UInt64 getReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
    int level = inLevel - __kremlin_min_level;
    assert(entry != NULL);
    return (level >= 0 && entry->timeArrayLength > level && entry->readVersion[level] == version) ?
                    entry->readTime[level] : 0;
}

void updateReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int level = inLevel - __kremlin_min_level;
    assert(entry != NULL);

    entry->readVersion[level] = version;
    entry->readTime[level] = timestamp;
}

void updateReadMemoryAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getReadTimestamp(entry, inLevel, version);

    if (prevTimestamp == 0LL) {
        regionInfo[region].readCnt++;
        //fprintf(stderr, "\t[load] addr = 0x%x level = %d version = %d timestamp = %d\n",
        //  entry, inLevel, version, timestamp);

        updateReadTimestamp(entry, inLevel, version, timestamp);
    }
}
    
// 1) update readCnt and writeCnt
// 2) update timestamp
void updateWriteMemoryAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].writeCnt++;
    }   
    //updateTimestamp(entry, inLevel, version, timestamp);
}

void updateReadMemoryLineAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getReadTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].readLineCnt++;
        //fprintf(stderr, "[line] addr = 0x%x level = %d version = %d timestamp = %d\n",
        //  entry, inLevel, version, timestamp);
        updateReadTimestamp(entry, inLevel, version, timestamp);

    }
}

void updateWriteMemoryLineAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].writeLineCnt++;
    }
    TEntryUpdate(entry, inLevel, version, timestamp);
}
#endif

UInt MShadowInit() {
	//GTableCreate(&gTable);	
	STableInit();
}


UInt MShadowDeinit() {
	//GTableDelete(&gTable);
	printMemStat();
	STableDeinit();
}

static Time array[128];
Time* MShadowGet(Addr addr, Index size, Version* vArray) {
	Index i;
	MSG(0, "MShadowGet 0x%llx, size %d\n", addr, size);
	for (i=0; i<size; i++)
		array[i] = getTime(addr, i, vArray[i]);
	return array;
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray) {
	Index i;
	MSG(0, "MShadowSet 0x%llx, size %d\n", addr, size);
	for (i=0; i<size; i++)
		setTime(addr, i, vArray[i], tArray[i]);
}

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


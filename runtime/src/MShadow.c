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

// 128GB addr space will be enough
typedef struct _STable {
	SEntry entry[32];	
} STable;




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
	GTableCreate(&gTable);	
}


UInt MShadowDeinit() {
	GTableDelete(&gTable);
}

void MShadowGet(Addr addr, Index size, Version* vArray, Time* tArray) {
	Index i;
	for (i=0; i<size; i++)
		tArray[i] = 0ULL;
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray) {
	return;
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


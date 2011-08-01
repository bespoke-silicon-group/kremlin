#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "defs.h"
#include "debug.h"
//#include "table.h"
#include "hash_map.h"
#include "cregion.h"
#include "Vector.h"
#include "MShadow.h"

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)
#define DS_ALLOC_SIZE   100     // used for static data structures
#define MAX_SRC_TSA_VAL	6
#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

static GTable* gTable;


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
 * Typedefs
 */
typedef struct GTEntry GTEntry;

// TODO: reference count pages properly and free them when no longer used.
/*
 * Prototypes
 */
static int GTEntryCreate(GTEntry** e);
static int GTEntryDelete(GTEntry** e);
static int GTEntryDeleteTEntry(GTEntry*, Addr addr);
static TEntry** GTEntryGet(GTEntry* e, Addr addr);
static GTEntry** GTableGetGTEntry(GTable* t, Addr addr);
static UInt64 GTableIndex(Addr addr);
static UInt64 GTEntryIndex(Addr addr);

/*
 * Struct definitions
 */
struct GTEntry {
    unsigned short used; // number of entries that are in use
    unsigned short usedLine; // number of entries that are in use
    TEntry* array[L2_SIZE];
    TEntry* lineArray[L2_SIZE >> CACHE_LINE_POWER_2];
};

/*  
    GlobalTable:
        global table is a hashtable with lower address as its primary key.
*/
struct GTable {
    GTEntry* array[L1_SIZE];
};

int GTEntryCreate(GTEntry** e)
{
    if(!(*e = (GTEntry*)calloc(sizeof(GTEntry), 1)))
    {
        assert(0 && "calloc returned NULL in GTEntryCreate");
        return FALSE;
    }
    return TRUE;
}

int GTEntryDelete(GTEntry** e)
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

int GTEntryDeleteTEntry(GTEntry* e, Addr addr)
{
    TEntry** tEntry = GTEntryGet(e, addr);
    TEntryFree(*tEntry);
    *tEntry = NULL;

    e->used--;
}

TEntry** GTEntryGet(GTEntry* e, Addr addr)
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

UInt64 GTEntryIndex(Addr addr)
{
    return ((UInt64)addr >> L2_SHIFT) & L2_MASK;
}

int GTableCreate(GTable** t)
{
    if(!(*t = (GTable*)calloc(1, sizeof(GTable))))
    {
        assert(0 && "calloc failed");
        return FALSE;
    }
    return TRUE;
}

int GTableDelete(GTable** t) {
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
TEntry* GTableGetTEntry(GTable* t, Addr addr) 
{
#ifndef WORK_ONLY
    GTEntry** entry = GTableGetGTEntry(t, addr);
    return *GTEntryGet(*entry, addr);
#else
	return (TEntry*)1;
#endif
}

// get GTEntry in GTable t at address addr
GTEntry** GTableGetGTEntry(GTable* t, Addr addr)
{
    UInt32 index = GTableIndex(addr);
	GTEntry** entry = t->array + index;
	if(*entry == NULL)
		GTEntryCreate(entry);
    return entry;
}

int GTableDeleteTEntry(GTable* t, Addr addr)
{
    GTEntry** entry = GTableGetGTEntry(t, addr);
    GTEntryDeleteTEntry(*entry, addr);
    return TRUE;
}

UInt64 GTableIndex(Addr addr)
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


UInt MShadowFinalize() {
	GTableDelete(&gTable);
}


Timestamp memGetTimestamp(Addr addr, Index index) {	
	Version version = versionGet(index);
	TEntry* entry = GTableGetTEntry(gTable, addr);
	Timestamp ts = TEntryGet(entry, index, version);
	return ts;
}

void memSetTimestamp(Timestamp time, Addr addr, Index index) {
	Version version = versionGet(index);
	TEntry* entry = GTableGetTEntry(gTable, addr);
	TEntryUpdate(entry, index, version, time);
}


#if 0
// Sat's version from here...
// move to table.c
void fillTSArray(TEntry* src_entry, TSArray* dest_tsa) {
	int fill_size = MIN(src_entry->timeArrayLength,numActiveLevels);

	// Fill in the data, taking into account version
	int i;
	for(i = 0; i < fill_size; ++i) {
    	dest_tsa->times[i] = (src_entry->version[i] == versions[i]) ?  src_entry->time[i] : 0;
	}

	// TODO: memset for this?
	for(i = fill_size; i < numActiveLevels; ++i) { dest_tsa->times[i] = 0; }
}

/*
 * Filles tsa array with timestamps associated with vreg
 * @param vreg			Virtual register number to get data from
 * @param tsa			TSArray to write TS data to
 */
// TODO: move to table.c
void getLocalTimes(UInt src_vreg, TSArray* tsa) {
	TEntry* entry = getLTEntry(src_vreg);

	fillTSArray(entry, tsa);
}

// TODO: move to GTable.c
void getGlobalTimes(Addr src_addr, TSArray* tsa) {
    TEntry* entry = GTableGetTEntry(gTable, src_addr);

	fillTSArray(entry, tsa);
}

void getCDTTSArray(TSArray* tsa) {
	int i;
	for(i = 0; i < numActiveLevels; ++i) {
		tsa->times[i] = (cdtHead->version[i] == versions[i]) ?  cdtHead->time[i] : 0;
	}
}

// precondition: numActiveLevels > 0
TSArray* getSrcTSArray(UInt idx) { 
	assert(idx <= MAX_SRC_TSA_VAL);

	// TODO: move this if to logRegionEntry for more efficiency
	if(src_arrays_size < numActiveLevels) {
		src_arrays = realloc(src_arrays,sizeof(TSArray*)*numActiveLevels);

		for( ; src_arrays_size < numActiveLevels; ++src_arrays_size) {
			src_arrays[src_arrays_size] = malloc(sizeof(TSArray)*MAX_SRC_TSA_VAL);

			int i;
			for(i = 0; i < MAX_SRC_TSA_VAL; ++i) {
				src_arrays[src_arrays_size][i].times =
					malloc(sizeof(Timestamp)*(src_arrays_size+1));
				src_arrays[src_arrays_size][i].size = src_arrays_size+1;
			}
		}
	}

	return &src_arrays[numActiveLevels-1][idx];
}

// precondition: numActiveLevels > 0
TSArray* getDestTSArray() { 
	// TODO: move this if to logRegionEntry for more efficiency
	if(dest_arrays_size < numActiveLevels) {
		dest_arrays = realloc(dest_arrays,sizeof(TSArray)*numActiveLevels);

		for( ; dest_arrays_size < numActiveLevels; ++dest_arrays_size) {
			dest_arrays[dest_arrays_size].times = 
				malloc(sizeof(Timestamp)*(dest_arrays_size+1));
			dest_arrays[dest_arrays_size].size = dest_arrays_size+1;
		}
	}

	return &dest_arrays[numActiveLevels-1];
}

// TODO: move to GTable.c
void updateTEntry(TEntry* entry, TSArray* src_tsa) {
    TEntryAllocAtLeastLevel(entry, 0);

	int i;
	for(i = 0; i < numActiveLevels; ++i) {
		entry->time[i] = src_tsa->times[i];
		regionInfo[i].cp = MAX(src_tsa->times[i],regionInfo[i].cp); // update CP

		entry->version[i] = versions[i];
	}
}

// TODO: move to table.c
void updateLocalTimes(UInt dest_vreg, TSArray* src_tsa) {
	TEntry* entry = getLTEntry(dest_vreg);

	updateTEntry(entry, src_tsa);
}

// TODO: move to GTable.c
void updateGlobalTimes(Addr dest_addr, TSArray* src_tsa) {
    TEntry* entry = GTableGetTEntry(gTable, dest_addr);

	updateTEntry(entry, src_tsa);
}

// TODO: move to GTable.c
void eraseGlobalTimes(Addr addr) {
	GTableDeleteTEntry(gTable, addr);
}

#endif

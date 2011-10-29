#include <assert.h>
#include "table.h"
#include "hash_map.h"
#include "MemMapAllocator.h"

#include "globals.h"

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

TSArray* getTSArrayLocal(UInt vreg);

// BEGIN OLD STUFF

HASH_MAP_DEFINE_PROTOTYPES(mt, Addr, size_t);
HASH_MAP_DEFINE_FUNCTIONS(mt, Addr, size_t);

LTable* lTable;
static hash_map_mt* mTable;

Pool* tEntryPool; // TODO: deprecated???

void dumpTableMemAlloc(void);
static UInt64 addrHash(Addr addr);
static int addrCompare(Addr a1, Addr a2);

void freeTEntry(TEntry* entry);

UInt32 getTEntrySize() { return __kremlin_max_profiled_level; }

long long _tEntryLocalCnt = 0;
long long _tEntryGlobalCnt = 0;
extern int levelNum;

UInt64 __kremlin_overhead_in_words;
UInt64 __kremlin_max_overhead_in_words;

// XXX: for PoolMalloc to work, size always must be the same!
TEntry* allocTEntry() {
    TEntry* entry;
    
    if(!(entry = (TEntry*) malloc(sizeof(TEntry))))
    {
        fprintf(stderr, "Failed to alloc TEntry\n");
        assert(0);
        return NULL;
    }

	__kremlin_overhead_in_words += 5;

	if(levelNum >= 0 && levelNum >= __kremlin_min_level) {
		UInt32 max_level = MIN(__kremlin_max_profiled_level,levelNum);
		UInt32 size = (max_level - __kremlin_min_level) + 1;

    	entry->version = (UInt32*)calloc(sizeof(UInt32), size);
    	entry->time = (UInt64*)calloc(sizeof(UInt64), size);

    	entry->timeArrayLength = size;

		__kremlin_overhead_in_words += size * 3;
	}
	else {
		entry->version = 0;
		entry->time = 0;
		entry->timeArrayLength = 0;
	}

	__kremlin_max_overhead_in_words = (__kremlin_overhead_in_words > __kremlin_max_overhead_in_words) ? __kremlin_overhead_in_words : __kremlin_max_overhead_in_words;

#ifdef EXTRA_STATS
	// FIXME: overprovisioning
    entry->readVersion = (UInt32*)calloc(sizeof(UInt32), levelNum+1);
    entry->readTime = (UInt64*)calloc(sizeof(UInt64), levelNum+1);
#endif /* EXTRA_STATS */

	//fprintf(stderr,"alloc tentry with (levelNum,min_level,timearraylength) = (%d,%u,%u)\n",
		//levelNum,__kremlin_min_level,entry->timeArrayLength);

    return entry;
}

/**
 * Allocates enough memory for at least the specified level.
 * @param entry The TEntry.
 * @param level The level.
 */
void TEntryAllocAtLeastLevel(TEntry* entry, UInt32 level)
{
	UInt32 max_level = MIN(__kremlin_max_profiled_level,levelNum);
	UInt32 new_size = (max_level - __kremlin_min_level) + 1;

    if(levelNum >= __kremlin_min_level 
	  && entry->timeArrayLength <= new_size)
    {
        UInt32 lastSize = entry->timeArrayLength; // moved here overhead logging

        entry->time = (UInt64*)realloc(entry->time, sizeof(UInt64) * new_size);
        entry->version = (UInt32*)realloc(entry->version, sizeof(UInt32) * new_size);

		__kremlin_overhead_in_words += (new_size - lastSize) * 3;

		__kremlin_max_overhead_in_words = (__kremlin_overhead_in_words > __kremlin_max_overhead_in_words) ? __kremlin_overhead_in_words : __kremlin_max_overhead_in_words;

        assert(entry->time);
        assert(entry->version);


#ifdef EXTRA_STATS

		// FIXME: overprovisioning
        entry->readTime = (UInt64*)realloc(entry->readTime, sizeof(UInt64) * (level+1));
        entry->readVersion = (UInt32*)realloc(entry->readVersion, sizeof(UInt32) * (level+1));

		// FIXME! should this be level+1 - lastSize???
        bzero(entry->version + lastSize, sizeof(UInt32) * (level - lastSize));

        assert(entry->readTime);
        assert(entry->readVersion);

        bzero(entry->readTime + lastSize, sizeof(UInt64) * (level - lastSize));
        bzero(entry->readVersion + lastSize, sizeof(UInt32) * (level - lastSize));

#endif /* EXTRA_STATS */

        entry->timeArrayLength = new_size;
    }
}

void freeTEntry(TEntry* entry) {
    if(!entry) return;

	__kremlin_overhead_in_words -= 5 + (3 * entry->timeArrayLength);

    free(entry->version);
    free(entry->time);

#ifdef EXTRA_STATS
    free(entry->readVersion);
    free(entry->readTime);
#endif /* EXTRA_STATS */

	free(entry);
	//PoolFree(tEntryPool, entry);
}

void createMEntry(Addr addr, size_t size) {
    hash_map_mt_put(mTable, addr, size, TRUE);
}

void freeMEntry(Addr start_addr) {
    hash_map_mt_remove(mTable, start_addr);
}

//TODO: rename to something more accurate
size_t getMEntry(Addr addr) {
    size_t* ret = hash_map_mt_get(mTable, addr);
    if(!ret)
    {
        fprintf(stderr, "Freeing %p which was never malloc'd!\n", addr);
        assert(0);
    }
    return *ret;
}

void copyTEntry(TEntry* dest, TEntry* src) {
	assert(dest != NULL);
	assert(src != NULL);

    TEntryAllocAtLeastLevel(dest, src->timeArrayLength);

    assert(dest->timeArrayLength >= src->timeArrayLength);

    memcpy(dest->version, src->version, src->timeArrayLength * sizeof(UInt64));
    memcpy(dest->time, src->time, src->timeArrayLength * sizeof(UInt32));
}

LTable* allocLocalTable(int size) {
	LTable* ret = (LTable*) malloc(sizeof(LTable));
	ret->size = size;
	ret->array = (TEntry**) malloc(sizeof(TEntry*) * size);

	int i;	
	for (i=0; i<size; i++) {
		ret->array[i] = allocTEntry();
	}
	_tEntryLocalCnt += size;

//	printf("Alloc LTable to 0x%x\n", ret);
	return ret;
}


void freeLocalTable(LTable* table) {
	assert(table != NULL);
	assert(table->array != NULL);

	int i;
	for (i=0; i<table->size; i++) {
		assert(table->array[i] != NULL);
		freeTEntry(table->array[i]);
	}
	_tEntryLocalCnt -= table->size;
	free(table->array);
	free(table);
}

// Why do we use a global variable to reference the local table? Shouldn't we
// just use the one in FuncContext? --chris
void setLocalTable(LTable* table) {
//	printf("Set LTable to 0x%x\n", table);
	lTable = table;
}

void initDataStructure(int regionLevel) {
	//fprintf(stderr, "[kremlin] # of instrumented levels = %d\n", regionLevel);
	//maxRegionLevel = regionLevel;
    hash_map_mt_create(&mTable, addrHash, addrCompare, NULL, NULL);

	// Set TEntry Size
    size_t versionSize = sizeof(UInt32) * regionLevel;
    size_t timeSize = sizeof(UInt64) * regionLevel;
    size_t spaceToAlloc = sizeof(TEntry) + versionSize + timeSize;

#ifdef EXTRA_STATS
    size_t readVersionSize = sizeof(UInt32) * regionLevel;
    size_t readTimeSize = sizeof(UInt64) * regionLevel;
    spaceToAlloc += readVersionSize + readTimeSize;
#endif
	PoolCreate(&tEntryPool, spaceToAlloc, memPool, (void*(*)(void*, size_t))MemMapAllocatorMalloc);
}

void finalizeDataStructure() {
    hash_map_mt_delete(&mTable);
    PoolDelete(&tEntryPool);
}

void printTEntry(TEntry* entry) {
	int i;
	for (i = 0; i < getTEntrySize(); i++) {
		printf("%u %lld\n", entry->version[i], entry->time[i]);
	}
}

// preconditions: lTable != NULL
TEntry* getLTEntry(UInt32 vreg) {
#ifndef WORK_ONLY
	if (vreg >= lTable->size) {
		fprintf(stderr,"ERROR: vreg = %lu, lTable size = %d\n", vreg, lTable->size);
		assert(0);
	}
	//assert(vreg < lTable->size);
	return lTable->array[vreg];	
#else
	return (TEntry*)1;
#endif
}

void dumpTableMemAlloc() {
}

UInt64 addrHash(Addr addr)
{
    return addr;
}
int addrCompare(Addr a1, Addr a2)
{
    return a1 == a2;
}


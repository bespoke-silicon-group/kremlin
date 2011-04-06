#include <assert.h>
#include "table.h"
#include "hash_map.h"
#include "MemMapAllocator.h"

HASH_MAP_DEFINE_PROTOTYPES(mt, Addr, size_t);
HASH_MAP_DEFINE_FUNCTIONS(mt, Addr, size_t);

LTable* lTable;
UInt32	maxRegionLevel;
static hash_map_mt* mTable;
Pool* tEntryPool;

void dumpTableMemAlloc(void);
static UInt64 addrHash(Addr addr);
static int addrCompare(Addr a1, Addr a2);

UInt32 getTEntrySize() {
	return maxRegionLevel;
}


void freeTEntry(TEntry* entry);

/*
MTable* allocMallocTable() {
	MTable* ret = (MTable*) calloc(1,sizeof(MTable));

	ret->size = -1;
	ret->array = calloc(MALLOC_TABLE_CHUNK_SIZE,sizeof(MEntry*));
	ret->capacity = MALLOC_TABLE_CHUNK_SIZE;
	return ret;
}

void freeMallocTable(MTable* table) {
	if(table->size > 0) {
		fprintf(stderr,"WARNING: %d entries left in malloc table. Should be 0.\n",table->size);
	}

	int i;
	for(i = 0; i < table->size; ++i) {
		free(table->array[i]);
	}

	free(table->array);
	free(table);
}
*/

long long _tEntryLocalCnt = 0;
long long _tEntryGlobalCnt = 0;
extern UInt levelNum;
// XXX: for PoolMalloc to work, size always must be the same!
TEntry* allocTEntry(int size) {
    TEntry* entry;
    
    if(!(entry = (TEntry*) malloc(sizeof(TEntry))))
    {
        fprintf(stderr, "Failed to alloc TEntry\n");
        assert(0);
        return NULL;
    }

    entry->version = (UInt32*)calloc(sizeof(UInt32), levelNum);
    entry->time = (UInt64*)calloc(sizeof(UInt64), levelNum);

#ifdef EXTRA_STATS
    entry->readVersion = (UInt32*)calloc(sizeof(UInt32), levelNum);
    entry->readTime = (UInt64*)calloc(sizeof(UInt64), levelNum);
#endif /* EXTRA_STATS */

    entry->timeArrayLength = levelNum;

    return entry;
}

/**
 * Allocates enough memory for at least the specified level.
 * @param entry The TEntry.
 * @param level The level.
 */
void TEntryAllocAtLeastLevel(TEntry* entry, UInt32 level)
{
    if(entry->timeArrayLength < level)
    {
        entry->time = (UInt64*)realloc(entry->time, sizeof(UInt64) * level);
        entry->version = (UInt32*)realloc(entry->version, sizeof(UInt32) * level);

        assert(entry->time);
        assert(entry->version);


#ifdef EXTRA_STATS

        UInt32 lastSize = entry->timeArrayLength;
        entry->readTime = (UInt64*)realloc(entry->readTime, sizeof(UInt64) * level);
        entry->readVersion = (UInt32*)realloc(entry->readVersion, sizeof(UInt32) * level);

        bzero(entry->version + lastSize, sizeof(UInt32) * (level - lastSize));

        assert(entry->readTime);
        assert(entry->readVersion);

        bzero(entry->readTime + lastSize, sizeof(UInt64) * (level - lastSize));
        bzero(entry->readVersion + lastSize, sizeof(UInt32) * (level - lastSize));

#endif /* EXTRA_STATS */

        entry->timeArrayLength = level;
    }
}

void freeTEntry(TEntry* entry) {
    if(!entry)
        return;

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
//	MEntry* me = (MEntry*)malloc(sizeof(MEntry));
//
//	me->start_addr = start_addr;
//	me->size = entry_size;
//
//	int mtable_size = mTable->size + 1;
//
//	//assert(mtable_size < MALLOC_TABLE_CHUNK_SIZE);
//
//	// see if we need to create more entries for malloc table
//	if(mtable_size == mTable->capacity) {
//		int new_mtable_capacity = mtable_size + MALLOC_TABLE_CHUNK_SIZE;
//		fprintf(stderr, "Increasing size of malloc table from %d to %d\n",mtable_size,new_mtable_capacity);
//		void* old = mTable->array;
//		mTable->array = realloc(mTable->array,(mtable_size+MALLOC_TABLE_CHUNK_SIZE) * sizeof(MEntry*));
//		fprintf(stderr, "mTable from 0x%x to 0x%x\n", old, mTable->array);
//		mTable->capacity = new_mtable_capacity;
//		/*
//		int i;
//		for(i = mtable_size; i < new_mtable_capacity; ++i) {
//			mTable->array[i] = NULL;
//		}
//		*/
//	}
//
//	mTable->size = mtable_size;
//	mTable->array[mtable_size] = me;
}

void freeMEntry(Addr start_addr) {
    hash_map_mt_remove(mTable, start_addr);

//	MEntry* me = NULL;
//
//	// most likely to free something we recently malloced
//	// so we'll start searching from the end
//	int i, found_index;
//	for(i = mTable->size; i >= 0; --i) {
//		if(mTable->array[i]->start_addr == start_addr) {
//			me = mTable->array[i];
//			found_index = i;
//			break;
//		}
//	}
//
//	assert(me != NULL);
//
//	// need to shuffle entries accordingly now that we
//	// are going to delete this entry
//	if(found_index != mTable->size) {
//		// starting from found index, shift everything
//		// to the left by one spot
//		for(i = found_index; i < mTable->size; ++i) {
//			mTable->array[i] = mTable->array[i+1];
//		}
//	}
//
//	// NULLIFIED!
//	free(me);
//	mTable->array[mTable->size] = NULL;
//
//	mTable->size -= 1;
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

//	MEntry* me = NULL;
//
//	// search from end b/c we're most likely to find it there
//	int i;
//	for(i = mTable->size; i >= 0; --i) {
//		if(mTable->array[i]->start_addr == start_addr) {
//			me = mTable->array[i];
//			break;
//		}
//	}
//
//	if(me == NULL) {
//		fprintf(stderr,"no entry found with addr 0x%p. mTable->size = %d\n",start_addr,mTable->size);
//		assert(0);
//	}
//
//	return me;
}

void copyTEntry(TEntry* dest, TEntry* src) {
	int i;
	assert(dest != NULL);
	assert(src != NULL);
    TEntryAllocAtLeastLevel(dest, src->timeArrayLength);
    assert(dest->timeArrayLength >= src->timeArrayLength);
    memcpy(dest->version, src->version, src->timeArrayLength * sizeof(UInt64));
    memcpy(dest->time, src->time, src->timeArrayLength * sizeof(UInt32));
}

LTable* allocLocalTable(int size) {
	int i;	
	LTable* ret = (LTable*) malloc(sizeof(LTable));
	ret->size = size;
	ret->array = (TEntry**) malloc(sizeof(TEntry*) * size);
	for (i=0; i<size; i++) {
		ret->array[i] = allocTEntry(getTEntrySize());
	}
	_tEntryLocalCnt += size;

//	printf("Alloc LTable to 0x%x\n", ret);
	return ret;
}


void freeLocalTable(LTable* table) {
	int i;
	assert(table != NULL);
	assert(table->array != NULL);
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
	fprintf(stderr, "# of instrumented region Levels = %d\n", regionLevel);
	maxRegionLevel = regionLevel;
    hash_map_mt_create(&mTable, addrHash, addrCompare, NULL, NULL);

	// Set TEntry Size
    size_t versionSize = sizeof(UInt32) * maxRegionLevel;
    size_t timeSize = sizeof(UInt64) * maxRegionLevel;
    size_t spaceToAlloc = sizeof(TEntry) + versionSize + timeSize;

#ifdef EXTRA_STATS
    size_t readVersionSize = sizeof(UInt32) * maxRegionLevel;
    size_t readTimeSize = sizeof(UInt64) * maxRegionLevel;
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

#if 0
extern int regionNum;
char* toStringTEntry(TEntry* entry) {
#if KREMLIN_DEBUG == 1
    int level = regionNum;
    int i;
    char temp[50];

    // XXX: Causes memory leak?
    char* ret = (char*)malloc(300);
    ret[0] = 0;

    for (i = 0; i < level; i++) {
        sprintf(temp, " %llu (%u)", entry->time[i], entry->version[i]);
        strcat(ret, temp);
    }
    return ret;
#endif
}
#endif

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


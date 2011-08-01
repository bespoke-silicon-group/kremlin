#include <assert.h>
#include "TEntry.h"
//#include "hash_map.h"
//#include "MemMapAllocator.h"
//#include "globals.h"

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

//HASH_MAP_DEFINE_PROTOTYPES(mt, Addr, size_t);
//HASH_MAP_DEFINE_FUNCTIONS(mt, Addr, size_t);


/*
void 	dumpTableMemAlloc(void);
static 	UInt64 addrHash(Addr addr);
static 	int addrCompare(Addr a1, Addr a2);

void freeTEntry(TEntry* entry);
*/



inline Timestamp TEntryGet(TEntry* entry, Index index, Version version) {
    Timestamp ret = (index >= 0 && index < entry->depth && entry->version[index] == version) ?
				entry->time[index] : 0;
	return ret;
}


inline void TEntryUpdate(TEntry* entry, Index index, Version version, Timestamp timestamp) {
	assert(entry != NULL);
	TEntryRealloc(entry, index);
	entry->version[index] = version;
	entry->time[index] = timestamp;
}


TEntry* TEntryAlloc(Index depth) {
    TEntry* entry;
    
    if(!(entry = (TEntry*) malloc(sizeof(TEntry))))
    {
        fprintf(stderr, "Failed to alloc TEntry\n");
        assert(0);
    }
   	entry->version = (Version*)calloc(sizeof(Version), depth);
   	entry->time = (Timestamp*)calloc(sizeof(Timestamp), depth);
   	entry->depth = depth;

#ifdef EXTRA_STATS
#if 0
	// FIXME: overprovisioning
    entry->readVersion = (UInt32*)calloc(sizeof(UInt32), depth);
    entry->readTime = (UInt64*)calloc(sizeof(UInt64), depth);
#endif
#endif /* EXTRA_STATS */

	//fprintf(stderr,"alloc tentry with (levelNum,min_level,timearraylength) = (%d,%u,%u)\n",
		//levelNum,__kremlin_min_level,entry->depth);
    return entry;
}

// TODO: level parameter should go away
/**
 * Allocates enough memory for at least the specified level.
 * @param entry The TEntry.
 * @param newDepth The required depth.
 */
void TEntryRealloc(TEntry* entry, Index newDepth)
{
    if(entry->depth < newDepth)
    {
        UInt32 lastSize = entry->depth;

        entry->time = (UInt64*)realloc(entry->time, sizeof(UInt64) * newDepth);
        entry->version = (UInt32*)realloc(entry->version, sizeof(UInt32) * newDepth);

        assert(entry->time);
        assert(entry->version);

#ifdef EXTRA_STATS
#if 0
		// FIXME: overprovisioning
        entry->readTime = (UInt64*)realloc(entry->readTime, sizeof(UInt64) * newDepth);
        entry->readVersion = (UInt32*)realloc(entry->readVersion, sizeof(UInt32) * newDepth);
        bzero(entry->version + lastSize, sizeof(UInt32) * (level - lastSize));

        assert(entry->readTime);
        assert(entry->readVersion);

        bzero(entry->readTime + lastSize, sizeof(UInt64) * (level - lastSize));
        bzero(entry->readVersion + lastSize, sizeof(UInt32) * (level - lastSize));
#endif
#endif /* EXTRA_STATS */

        entry->depth = newDepth;
    }
}

void TEntryFree(TEntry* entry) {
    if(!entry) return;

    free(entry->version);
    free(entry->time);

#ifdef EXTRA_STATS
#if 0
    free(entry->readVersion);
    free(entry->readTime);
#endif
#endif /* EXTRA_STATS */

	free(entry);
	//PoolFree(tEntryPool, entry);
}

void TEntryCopy(TEntry* dest, TEntry* src) {
	assert(dest != NULL);
	assert(src != NULL);

    TEntryRealloc(dest, 0); // level isn't used anymore, so we 0 it

    assert(dest->depth >= src->depth);
    memcpy(dest->version, src->version, src->depth * sizeof(Version));
    memcpy(dest->time, src->time, src->depth * sizeof(Timestamp));
}
#if 0
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


#endif

/**
 * Prints out TEntry times.
 * @param entry		Pointer to TEntry.
 * @param size		Number of entries to print
 */
void TEntryDump(TEntry* entry, int size) {
    int i;
    fprintf(stderr, "entry@%p\n", entry);
	// XXX: assert on bounds check?
    for (i = 0; i < size; i++) {
        fprintf(stderr, "\t%llu", entry->time[i]);
    }
    fprintf(stderr, "\n");
}


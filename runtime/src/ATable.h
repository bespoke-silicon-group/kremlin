#ifndef TABLE_H
#define TABLE_H

#include "Pool.h"
#include "defs.h"

#ifndef MALLOC_TABLE_CHUNK_SIZE
//#define MALLOC_TABLE_CHUNK_SIZE   8192
#define MALLOC_TABLE_CHUNK_SIZE   16384
#endif

#define CACHE_LINE_POWER_2  4
#define CACHE_LINE_SIZE     (1 << CACHE_LINE_POWER_2)

typedef struct _MTableEntry {
    Addr start_addr;
    size_t size;
} MEntry;

// declaration of functions in table.c



/*
#if KREMLIN_DEBUG == 1
	char* toStringTEntry(TEntry* entry);
#else
#	define toStringTEntry(entry)       ((void)0)
#endif
*/

#endif /* TABLE_H */

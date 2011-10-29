#ifndef TABLE_H
#define TABLE_H

#include "Pool.h"
#include "defs.h"

#ifndef MALLOC_TABLE_CHUNK_SIZE
#define MALLOC_TABLE_CHUNK_SIZE   16384
#endif

#define CACHE_LINE_POWER_2  4
#define CACHE_LINE_SIZE     (1 << CACHE_LINE_POWER_2)

typedef Timestamp UInt64;
typedef VersionNumber UInt32;

typedef struct TSEntry {
	Timestamp ts;
	VersionNumber vn;
} TSEntry;

typedef struct TSArray {
	Timestamp* time;
	UInt32 size;
} TSArray;


void cloneTimestamps(UInt dest_sreg, TSArray* src_array);

TSArray* getTSArrayLocal(UInt vreg);

// BEGIN OLD STUFF

typedef struct _DataEntry {
    UInt32* version;
    UInt64* time;
    UInt timeArrayLength;
#ifdef EXTRA_STATS
    UInt32* readVersion;
    UInt64* readTime;
#endif

} TEntry;


/*  
    LocalTable:
        local table uses virtual register number as its key
*/
typedef struct _LocalTable {
    int             size;
    TEntry**     array;

} LTable;

typedef struct _MTableEntry {
    Addr start_addr;
    size_t size;
} MEntry;

// declaration of functions in table.c
void setLocalTable(LTable* table);

UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version);
void copyTEntry(TEntry* dest, TEntry* src);
UInt32 getMaxRegionLevel();
void finalizeDataStructure();
void updateTimestamp(TEntry* entry, UInt32 level, UInt32 version, UInt64 timestamp);

TEntry* allocTEntry();
void freeTEntry(TEntry* entry);
LTable* allocLocalTable(int size);
void freeLocalTable(LTable* table);
TEntry* getLTEntry(UInt32 index);
void initDataStructure(int regionLevel);
void finalizeDataStructure();
UInt32 getTEntrySize(void);
void TEntryAllocAtLeastLevel(TEntry* entry, UInt32 level);

extern LTable* lTable;
extern UInt32  maxRegionLevel;
extern Pool* tEntryPool;


#endif /* TABLE_H */

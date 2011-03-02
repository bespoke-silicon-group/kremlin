#ifndef TABLE_H
#define TABLE_H

#include "Pool.h"

#ifndef MALLOC_TABLE_CHUNK_SIZE
//#define MALLOC_TABLE_CHUNK_SIZE   8192
#define MALLOC_TABLE_CHUNK_SIZE   16384
#endif

#define CACHE_LINE_POWER_2  4
#define CACHE_LINE_SIZE     (1 << CACHE_LINE_POWER_2)

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

typedef struct _GTableEntry {
    unsigned short used; // number of entries that are in use
    unsigned short usedLine; // number of entries that are in use
    TEntry* array[0x4000];
    TEntry* lineArray[0x4000 >> CACHE_LINE_POWER_2];
} GEntry;

/*  
    GlobalTable:
        global table is a hashtable with lower address as its primary key.
*/
typedef struct _GlobalTable {
    GEntry* array[0x10000];
} GTable;


typedef struct _MTableEntry {
    Addr start_addr;
    size_t size;
} MEntry;

/*
    MallocTable:
        malloc table is a table to track active mallocs
*/
typedef struct _MallocTable {
    int size;
	int capacity;
    MEntry** array;
} MTable;


typedef UInt    WorkTable;
typedef struct _RegionInfo {
    int         type;
    UInt        did;
    LTable      lTable;
    GTable      gTable;
    WorkTable   work;

} RegionInfo;

// declaration of functions in table.c
void setLocalTable(LTable* table);

UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version);
UInt64 getTimestampNoVersion(TEntry* entry, UInt32 level);
void copyTEntry(TEntry* dest, TEntry* src);
UInt32 getMaxRegionLevel();
void finalizeDataStructure();
void updateTimestamp(TEntry* entry, UInt32 level, UInt32 version, UInt64 timestamp);

GEntry* createGEntry();
void createMEntry(Addr start_addr, size_t entry_size);
MEntry* getMEntry(Addr start_addr);
void freeMEntry(Addr start_addr);

TEntry* allocTEntry(int size);
void freeTEntry(TEntry* entry);
LTable* allocLocalTable(int size);
void freeLocalTable(LTable* table);
TEntry* getLTEntry(UInt32 index);
TEntry* getGTEntry(Addr addr);
TEntry* getGTEntryCacheLine(Addr addr);
void initDataStructure(int regionLevel);
void finalizeDataStructure();
UInt32 getTEntrySize(void);
void TEntryAllocAtLeastLevel(TEntry* entry, UInt32 level);

extern GTable* gTable;
extern LTable* lTable;
extern MTable* mTable;
extern UInt32  maxRegionLevel;
extern Pool* tEntryPool;

#if KREMLIN_DEBUG == 1
	char* toStringTEntry(TEntry* entry);
#else
#	define toStringTEntry(entry)       ((void)0)
#endif

#endif /* TABLE_H */

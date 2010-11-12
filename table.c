#include <assert.h>
#include "defs.h"
#include "table.h"
#include "pool.h"

//#define MAX_TENTRIES 10000000ull
#define MAX_TENTRIES 1024*1024*8ull

GTable* gTable;
LTable* lTable;
MTable* mTable;
UInt32  maxRegionLevel;
static Pool* tEntryPool;

void dumpTableMemAlloc(void);

UInt32 getTEntrySize() {
    return maxRegionLevel;
}


void freeTEntry(TEntry* entry);

GTable* allocGlobalTable(int depth) {
    GTable* ret = (GTable*) malloc(sizeof(GTable));
    bzero(ret->array, sizeof(GTable));
    return ret;
}

MTable* allocMallocTable() {
    MTable* ret = (MTable*) calloc(1,sizeof(MTable));

    ret->size = -1;
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
}

void freeGlobalTable(GTable* table) {
    int i, j;
    for (i = 0; i < 0x10000; i++) {
        if (table->array[i] != NULL) {
            GEntry* entry = table->array[i];
            for (j = 0; j < 0x4000; j++) {
                if (entry->array[j] != NULL) {
                    TEntry* current = entry->array[j];
                    freeTEntry(current);    
                }
            }   
            free(entry);
        }
    }
    free(table);
}

long long _tEntryLocalCnt = 0;
long long _tEntryGlobalCnt = 0;

// XXX: for PoolMalloc to work, size always must be the same!
TEntry* allocTEntry(int size) {
    TEntry* entry;
    size_t versionSize = sizeof(UInt32) * size;
    size_t readVersionSize = sizeof(UInt32) * size;
    size_t timeSize = sizeof(UInt64) * size;
    size_t readTimeSize = sizeof(UInt64) * size;
    size_t spaceToAlloc = sizeof(TEntry) + versionSize + readVersionSize + timeSize + readTimeSize;
    
    //if(!(entry = (TEntry*) malloc(spaceToAlloc)))
    if(!(entry = (TEntry*) PoolMalloc(tEntryPool)))
    {
        fprintf(stderr, "Failed to alloc TEntry\n");
        assert(0);
        return NULL;
    }
	//fprintf(stderr, "bzero addr = 0x%llx, size = %lld\n", entry, spaceToAlloc);
    bzero(entry, spaceToAlloc); 
	//fprintf(stderr, "bzero2 addr = 0x%llx, size = %lld\n", entry, spaceToAlloc);

    entry->version = (UInt32*)((unsigned char*)entry + sizeof(TEntry));
    entry->readVersion = (UInt32*)((unsigned char*)entry->version + versionSize);
    entry->time = (UInt64*)((unsigned char*)entry->readVersion + readVersionSize);
    entry->readTime = (UInt64*)((unsigned char*)entry->time + timeSize);
    return entry;
}

void freeTEntry(TEntry* entry) {
    PoolFree(tEntryPool, entry);
}

void createMEntry(Addr start_addr, size_t entry_size) {
    MEntry* me = (MEntry*)malloc(sizeof(MEntry));

    me->start_addr = start_addr;
    me->size = entry_size;

    int mtable_size = mTable->size + 1;

    assert(mtable_size < MALLOC_TABLE_SIZE);
    mTable->size = mtable_size;

    mTable->array[mtable_size] = me;
}

void freeMEntry(Addr start_addr) {
    MEntry* me = NULL;

    // most likely to free something we recently malloced
    // so we'll start searching from the end
    int i, found_index;
    for(i = mTable->size; i >= 0; --i) {
        if(mTable->array[i]->start_addr == start_addr) {
            me = mTable->array[i];
            found_index = i;
            break;
        }
    }

    assert(me != NULL);

    // need to shuffle entries accordingly now that we
    // are going to delete this entry
    if(found_index != mTable->size) {
        // starting from found index, shift everything
        // to the left by one spot
        for(i = found_index; i < mTable->size; ++i) {
            mTable->array[i] = mTable->array[i+1];
        }
    }

    // NULLIFIED!
    free(me);
    mTable->array[mTable->size] = NULL;

    mTable->size -= 1;
}

MEntry* getMEntry(Addr start_addr) {
    MEntry* me = NULL;

    // search from end b/c we're most likely to find it there
    int i;
    for(i = mTable->size; i >= 0; --i) {
        if(mTable->array[i]->start_addr == start_addr) {
            me = mTable->array[i];
            break;
        }
    }

    if(me == NULL) {
        fprintf(stderr,"no entry found with addr 0x%x. mTable->size = %d\n",start_addr,mTable->size);
        assert(0);
    }

    return me;
}

GEntry* createGEntry() {
    GEntry* ret = (GEntry*) malloc(sizeof(GEntry));
    bzero(ret, sizeof(GEntry));
    return ret;
}

void copyTEntry(TEntry* dest, TEntry* src) {
    int i;
    assert(dest != NULL);
    assert(src != NULL);
    for (i=0; i<getTEntrySize(); i++) {
        dest->version[i] = src->version[i];
        dest->time[i] = src->time[i];
    }   
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
//  printf("Alloc LTable to 0x%x\n", ret);
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

void setLocalTable(LTable* table) {
//  printf("Set LTable to 0x%x\n", table);
    lTable = table;
}

void initDataStructure(int regionLevel) {
    fprintf(stderr, "# of instrumented region Levels = %d\n", regionLevel);
    maxRegionLevel = regionLevel;
    gTable = allocGlobalTable(maxRegionLevel);
    mTable = allocMallocTable();
   
    size_t size = regionLevel;
    size_t versionSize = sizeof(UInt32) * size;
    size_t readVersionSize = sizeof(UInt32) * size;
    size_t timeSize = sizeof(UInt64) * size;
    size_t readTimeSize = sizeof(UInt64) * size;
    size_t spaceToAlloc = sizeof(TEntry) + versionSize + readVersionSize + timeSize + readTimeSize;
    PoolCreate(&tEntryPool, MAX_TENTRIES, spaceToAlloc);
}

void finalizeDataStructure() {
    freeGlobalTable(gTable);
    freeMallocTable(mTable);
    PoolDelete(&tEntryPool);
}

void printTEntry(TEntry* entry) {
    int i;
    for (i = 0; i < getTEntrySize(); i++) {
        printf("%u %lld\n", entry->version[i], entry->time[i]);
    }
}

TEntry* getLTEntry(UInt32 vreg) {
#ifndef WORK_ONLY
    assert(lTable != NULL);
    if (vreg >= lTable->size) {
        fprintf(stderr,"ERROR: vreg = %lu, lTable size = %d\n", vreg, lTable->size);
    }
    assert(vreg < lTable->size);
    
    return lTable->array[vreg]; 
#else
    return (TEntry*)1;
#endif
}

// FIXME: 64bit address?
TEntry* getGTEntry(Addr addr) {
#ifndef WORK_ONLY
    UInt32 index = ((UInt64) addr >> 16) & 0xffff;
    assert(index < 0x10000);
    GEntry* entry = gTable->array[index];
    if (entry == NULL) {
        entry = createGEntry();
        gTable->array[index] = entry;
    }
    UInt32 index2 = ((UInt64) addr >> 2) & 0x3fff;
    TEntry* ret = entry->array[index2];
    if (ret == NULL) {
        ret = allocTEntry(maxRegionLevel);
        entry->array[index2] = ret;
        entry->used += 1;
        _tEntryGlobalCnt++;
    }
    return ret;
#else
    return (TEntry*)1;
#endif
}

TEntry* getGTEntryCacheLine(Addr addr) {
#ifndef WORK_ONLY
    UInt32 index = ((UInt64) addr >> 16) & 0xffff;
    assert(index < 0x10000);
    GEntry* entry = gTable->array[index];
    if (entry == NULL) {
        entry = createGEntry();
        gTable->array[index] = entry;
    }
    UInt32 index2 = ((UInt64) addr >> (2 + CACHE_LINE_POWER_2)) & 0x3ff;
    TEntry* ret = entry->lineArray[index2];
    if (ret == NULL) {
        ret = allocTEntry(maxRegionLevel);
        entry->lineArray[index2] = ret;
        entry->usedLine += 1;
        //_tEntryGlobalCnt++;
    }
    return ret;
#else
    return (TEntry*)1;
#endif
}


void dumpTableMemAlloc() {
    fprintf(stderr, "local TEntry = %lld\n", _tEntryLocalCnt);
    fprintf(stderr, "global TEntry = %lld\n", _tEntryGlobalCnt);
}


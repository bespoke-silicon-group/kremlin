#include "defs.h"
#include "table.h"
#include "MemMapAllocator.h"

GTable* gTable;
LTable* lTable;
MTable* mTable;
UInt32	maxRegionLevel;
Pool* tEntryPool;

void dumpTableMemAlloc(void);

UInt32 getTEntrySize() {
	return maxRegionLevel;
}


void freeTEntry(TEntry* entry);

GTable* allocGlobalTable(int depth) {
	GTable* ret = (GTable*) calloc(sizeof(GTable), 1);
	return ret;
}

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
    size_t timeSize = sizeof(UInt64) * size;
    size_t spaceToAlloc = sizeof(TEntry) + versionSize + timeSize;

#ifdef EXTRA_STATS
    size_t readVersionSize = sizeof(UInt32) * size;
    size_t readTimeSize = sizeof(UInt64) * size;
    spaceToAlloc += readVersionSize + readTimeSize;
#endif
    
    //if(!(entry = (TEntry*) calloc(sizeof(unsigned char), spaceToAlloc)))
    if(!(entry = (TEntry*) PoolMalloc(tEntryPool)))
    {
        fprintf(stderr, "Failed to alloc TEntry\n");
        assert(0);
        return NULL;
    }
    //fprintf(stderr, "bzero addr = 0x%llx, size = %lld\n", entry, spaceToAlloc);
    assert(PoolGetPageSize(tEntryPool) >= spaceToAlloc);
    //fprintf(stderr, "bzero2 addr = 0x%llx, size = %lld\n", entry, spaceToAlloc);

    entry->version = (UInt32*)((unsigned char*)entry + sizeof(TEntry));
    entry->time = (UInt64*)((unsigned char*)entry->version + versionSize);

#ifdef EXTRA_STATS
    entry->readVersion = (UInt32*)((unsigned char*)entry->time + timeSize);
    entry->readTime = (UInt64*)((unsigned char*)entry->readVersion + readVersionSize);
#endif

    return entry;
}

void freeTEntry(TEntry* entry) {
    
	//free(entry);
	PoolFree(tEntryPool, entry);
}

void createMEntry(Addr start_addr, size_t entry_size) {
	MEntry* me = (MEntry*)malloc(sizeof(MEntry));

	me->start_addr = start_addr;
	me->size = entry_size;

	int mtable_size = mTable->size + 1;

	//assert(mtable_size < MALLOC_TABLE_CHUNK_SIZE);

	// see if we need to create more entries for malloc table
	if(mtable_size == mTable->capacity) {
		int new_mtable_capacity = mtable_size + MALLOC_TABLE_CHUNK_SIZE;
		fprintf(stderr, "Increasing size of malloc table from %d to %d\n",mtable_size,new_mtable_capacity);
		void* old = mTable->array;
		mTable->array = realloc(mTable->array,(mtable_size+MALLOC_TABLE_CHUNK_SIZE) * sizeof(MEntry*));
		fprintf(stderr, "mTable from 0x%x to 0x%x\n", old, mTable->array);
		mTable->capacity = new_mtable_capacity;
		/*
		int i;
		for(i = mtable_size; i < new_mtable_capacity; ++i) {
			mTable->array[i] = NULL;
		}
		*/
	}

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
		fprintf(stderr,"no entry found with addr 0x%p. mTable->size = %d\n",start_addr,mTable->size);
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
	gTable = allocGlobalTable(maxRegionLevel);
	mTable = allocMallocTable();

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

// preconditions: lTable != NULL
TEntry* getLTEntry(UInt32 vreg) {
#ifndef WORK_ONLY
		/*
	if (vreg >= lTable->size) {
		fprintf(stderr,"ERROR: vreg = %lu, lTable size = %d\n", vreg, lTable->size);
		assert(0);
	}*/
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

#if 0
extern int regionNum;
char* toStringTEntry(TEntry* entry) {
#if PYRPROF_DEBUG == 1
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
	fprintf(stderr, "local TEntry = %lld\n", _tEntryLocalCnt);
	fprintf(stderr, "global TEntry = %lld\n", _tEntryGlobalCnt);
}


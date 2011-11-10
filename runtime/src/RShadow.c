#include "Table.h"
#include "RShadow.h"
#include "MemMapAllocator.h"

/*
 * RShadow.c
 *
 * Implementation of register shadow. 
 * We use a simple 2D array for RShadow table (LTable).
 * 
 * 1) Unlike MShadow, RShadow does not use versioining 
 * becasue all register entries are going to be written 
 * and they should be cleaned before reused.
 * This allows low overhead shadow memory operation.
 *
 * 2) Unlike MShadow, RShaodw does not require dynamic resizing.
 * The size of a RShadow Table (LTable) is determined by
 * # of vregs and index depth - they are all available 
 * when the LTable is created.
 *
 * 3) If further optimization is desirable, 
 * it is possible to use a special memory allocator for 
 * LTable so that we can reduce calloc time from critical path.
 * However, I doubt if it will make a big impact,
 * as LTable creation is not a common operation compared to others.
 *
 */

static Table*	lTable;

static void initMemoryPool(Index depth) {
	//fprintf(stderr, "[kremlin] # of instrumented levels = %d\n", depth);
	//maxRegionLevel = depth;

	// Set TEntry Size
#if 0
    size_t versionSize = sizeof(Version) * depth;
    size_t timeSize = sizeof(Timestamp) * depth;
    size_t spaceToAlloc = sizeof(TEntry) + versionSize + timeSize;
#endif

#ifdef EXTRA_STATS
#if 0
    size_t readVersionSize = sizeof(UInt32) * depth;
    size_t readTimeSize = sizeof(UInt64) * depth;
    spaceToAlloc += readVersionSize + readTimeSize;
#endif
#endif
	//PoolCreate(&tEntryPool, spaceToAlloc, memPool, (void*(*)(void*, size_t))MemMapAllocatorMalloc);
}

static void finalizeMemoryPool() {
    //PoolDelete(&tEntryPool);
}

#if 0
// preconditions: lTable != NULL
TEntry* getLTEntry(Reg vreg) {
#ifndef WORK_ONLY
	if (vreg >= lTable->entrySize) {
		fprintf(stderr,"ERROR: vreg = %lu, lTable size = %d\n", vreg, lTable->entrySize);
		assert(0);
	}
	//assert(vreg < lTable->size);
	return lTable->array[vreg];	
#else
	return (TEntry*)1;
#endif
}
#endif


/*
 * Register Shadow Memory 
 */

UInt RShadowInit(Index depth) {
	initMemoryPool(depth);

}

UInt RShadowDeinit() {
	finalizeMemoryPool();
}


inline Time RShadowGetItem(Reg reg, Index index) {
	assert(index < getIndexSize());
	int offset = TableGetOffset(lTable, reg, index);
	Time ret = lTable->array[offset];
	return ret;
}

inline void RShadowSetItem(Time time, Reg reg, Index index) {
	int offset = TableGetOffset(lTable, reg, index);
	MSG(3, "RShadowSet: dest = 0x%x value = %d reg = %d index = %d offset = %d\n", 
		&(lTable->array[offset]), time, reg, index, offset);
	assert(lTable != NULL);
	assert(index < getIndexSize());
	assert(reg < lTable->row);
	lTable->array[offset] = time;
}


#if 0
inline Time* RShadowGet(Reg reg, Index size) {
	int offset = TableGetOffset(lTable, reg, 0);
	assert(index < getIndexSize());
	return &lTable->array[offset];
}

inline void RShadowSet(Reg reg, Index size, Time* tArray) {
	int offset = TableGetOffset(lTable, reg, 0);
	MSG(3, "RShadowSet: dest = 0x%x value = %d reg = %d index = %d offset = %d\n", 
		&(lTable->array[offset]), time, reg, index, offset);
	assert(lTable != NULL);
	assert(reg < lTable->row);
	memcpy(&lTable->array[offset], tArray, sizeof(Time) * size);
}
#endif


inline void RShadowActivateTable(Table* table) {
	//MSG(1, "Set Table to 0x%x\n", table);
	lTable = table;
#if 0
	int i;
	for (i=0; i<lTable->entrySize * lTable->indexSize; i++) {
		if (lTable->array[i] > 1000) {
			MSG(1, "\tElement %d is 0x%x\n", i, lTable->array[i]);
		}
	}
#endif
}

inline void RShadowRestartIndex(Index index) {
	Reg i;
	assert(lTable != NULL);
	for (i=0; i<lTable->row; i++) {
		RShadowSetItem(0ULL, i, index);
	}
}

inline Table* RShadowGetTable() {
	return lTable;
}

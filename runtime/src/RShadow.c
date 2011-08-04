#include "RShadow.h"
#include "MemMapAllocator.h"
#include "Pool.h"

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

static LTable*	lTable;
static Pool* 	tEntryPool;

static void initMemoryPool(Index depth) {
	//fprintf(stderr, "[kremlin] # of instrumented levels = %d\n", depth);
	//maxRegionLevel = depth;

	// Set TEntry Size
    size_t versionSize = sizeof(Version) * depth;
    size_t timeSize = sizeof(Timestamp) * depth;
    size_t spaceToAlloc = sizeof(TEntry) + versionSize + timeSize;

#ifdef EXTRA_STATS
#if 0
    size_t readVersionSize = sizeof(UInt32) * depth;
    size_t readTimeSize = sizeof(UInt64) * depth;
    spaceToAlloc += readVersionSize + readTimeSize;
#endif
#endif
	PoolCreate(&tEntryPool, spaceToAlloc, memPool, (void*(*)(void*, size_t))MemMapAllocatorMalloc);
}

static void finalizeMemoryPool() {
    PoolDelete(&tEntryPool);
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

Time RShadowGetWithTable(LTable* table, Reg reg, Index index) {
	assert(table != NULL);
	int offset = lTable->indexSize * reg + index;
	return table->array[offset];
}

void RShadowSetWithTable(LTable* table, Time time, Reg reg, Index index) {
	assert(table != NULL);
	int offset = lTable->indexSize * reg + index;
	table->array[offset] = time;
}

Time RShadowGet(Reg reg, Index index) {
	int offset = lTable->indexSize * reg + index;
	return lTable->array[offset];
}

void RShadowSet(Time time, Reg reg, Index index) {
	int offset = lTable->indexSize * reg + index;
	lTable->array[offset] = time;
}

/*
 * Copy values of a register to another table
 * It copies values for all indexes. 
 */
void RShadowCopy(LTable* destTable, Reg destReg, LTable* srcTable, Reg srcReg, Index start, Index size) {
	assert(destTable != NULL);
	assert(srcTable != NULL);
	int indexDest = destTable->indexSize * destReg + start;
	int indexSrc = srcTable->indexSize * srcReg + start;
	assert(size > 0);
	assert(start < destTable->indexSize);
	assert(start < srcTable->indexSize);

	memcpy(destTable + indexDest, srcTable + indexSrc, size * sizeof(Time));
}


#if 0
void RShadowExport(TArray* dest, Reg src) {
	Index i;
	/*
	for (i=0; i<getIndexSize(); i++)
		dest->values[i] = RShadowGet(src, i);
		*/
}


void RShadowImport(Reg dest, TArray* src) {
	Index i;
	/*
	for (i=0; i<getIndexSize(); i++) {
		RShadowSet(src->values[i], dest, i);
	}*/
}
#endif

LTable* RShadowCreateTable(int numEntry, Index depth) {
	LTable* ret = (LTable*) malloc(sizeof(LTable));
	ret->entrySize = numEntry;
	ret->indexSize = depth;
	// should be initialized with zero
	ret->array = (Time*) calloc(numEntry * depth, sizeof(Time));
	return ret;
}


void RShadowFreeTable(LTable* table) {
	assert(table != NULL);
	assert(table->array != NULL);

	free(table->array);
	free(table);
}

void RShadowActivateTable(LTable* table) {
//	printf("Set LTable to 0x%x\n", table);
	lTable = table;
}


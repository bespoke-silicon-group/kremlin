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
	Time ret = lTable->array[offset];
	assert(ret < 1000);
	return ret;
}

void RShadowSetWithTable(LTable* table, Time time, Reg reg, Index index) {
	assert(table != NULL);
	int offset = lTable->indexSize * reg + index;
	table->array[offset] = time;
	assert(time < 1000);
}

Time RShadowGet(Reg reg, Index index) {
	assert(index < getIndexSize());
	int offset = lTable->indexSize * reg + index;
	Time ret = lTable->array[offset];
	assert(ret < 1000);
	return ret;
}

void RShadowSet(Time time, Reg reg, Index index) {
	assert(lTable != NULL);
	assert(index < getIndexSize());
	int offset = lTable->indexSize * reg + index;
	lTable->array[offset] = time;
	MSG(3, "RShadowSet: dest = 0x%x value = %d reg = %d index = %d offset = %d\n", 
		&(lTable->array[offset]), time, reg, index, offset);
	assert(time < 1000);
}

/*
 * Copy values of a register to another table
 */
void RShadowCopy(LTable* destTable, Reg destReg, LTable* srcTable, Reg srcReg, Index start, Index size) {
	assert(destTable != NULL);
	assert(srcTable != NULL);
	int indexDest = destTable->indexSize * destReg + start;
	int indexSrc = srcTable->indexSize * srcReg + start;
	//MSG(0, "RShadowCopy: indexDest = %d,indexSrc = %d, start = %d, size = %d\n", indexDest, indexSrc, start, size);
	assert(size > 0);
	assert(start < destTable->indexSize);
	assert(start < srcTable->indexSize);

	Time* srcAddr = (Time*)(srcTable->array) + indexSrc;
	Time* destAddr = (Time*)(destTable->array) + indexDest;
	memcpy(destAddr, srcAddr, size * sizeof(Time));
	/*
	int i;
	for (i=0; i<size; i++) {
		Time value = *srcAddr++;
		*destAddr++ = value;
		MSG(1, "\tCopying from 0x%x to 0x%x value = %d\n", 
			srcAddr-1, destAddr-1, value);
	}*/

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
	ret->code = 0xDEADBEEF;
	//MSG(1, "RShadowCreateTable: ret = 0x%llx numEntry = %d, depth = %d\n", ret, numEntry, depth);
	return ret;
}


void RShadowFreeTable(LTable* table) {
	assert(table != NULL);
	assert(table->array != NULL);

	free(table->array);
	free(table);
}

void RShadowActivateTable(LTable* table) {
	//MSG(1, "Set LTable to 0x%x\n", table);
	lTable = table;
	assert(table->code == 0xDEADBEEF);
	int i;
	for (i=0; i<lTable->entrySize * lTable->indexSize; i++) {
		if (lTable->array[i] > 1000) {
			MSG(1, "\tElement %d is 0x%x\n", i, lTable->array[i]);
		}
	}
}


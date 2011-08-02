#include "RShadow.h"
#include "MemMapAllocator.h"
#include "Pool.h"

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

// preconditions: lTable != NULL
TEntry* getLTEntry(Reg vreg) {
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




/*
 * Register Shadow Memory 
 */

UInt RShadowInit(Index depth) {
	initMemoryPool(depth);

}

UInt RShadowFinalize() {
	finalizeMemoryPool();
}


Timestamp RShadowGetTimestamp(Reg reg, Index index) {
	Version version = versionGet(index);
	TEntry* entry = getLTEntry(reg);
	return TEntryGet(entry, index, version);
}

void RShadowSetTimestamp(Timestamp time, Reg reg, Index index) {
	Version version = versionGet(index);
	TEntry* entry = getLTEntry(reg);
	TEntryUpdate(entry, index, version, time);
}

void RShadowShadowToArg(Arg* dest, Reg src) {
	Index i;
	dest->reg = src;	
	for (i=0; i<getIndexSize(); i++)
		// can be specialized with the shadow memory implemetnation
		dest->values[i] = RShadowGetTimestamp(src, i);
}


void RShadowArgToShadow(Reg dest, Arg* src) {
	Index i;
	TEntry* entry = getLTEntry(dest);

	for (i=0; i<getIndexSize(); i++) {
		// how to handle versions?
		TEntryUpdate(entry, i, 0, src->values[i]);
	}
}

LTable* RShadowCreateTable(int numEntry, Index depth) {
	LTable* ret = (LTable*) malloc(sizeof(LTable));
	ret->size = numEntry;
	ret->array = (TEntry**) malloc(sizeof(TEntry*) * numEntry);

	int i;	
	for (i=0; i<numEntry; i++) {
		ret->array[i] = TEntryAlloc(depth);
	}
	//_tEntryLocalCnt += numEntry;

//	printf("Alloc LTable to 0x%x\n", ret);
	return ret;
}


void RShadowFreeTable(LTable* table) {
	assert(table != NULL);
	assert(table->array != NULL);

	int i;
	for (i=0; i<table->size; i++) {
		assert(table->array[i] != NULL);
		TEntryFree(table->array[i]);
	}
	//_tEntryLocalCnt -= table->size;
	free(table->array);
	free(table);
}

void RShadowSetActiveLTable(LTable* table) {
//	printf("Set LTable to 0x%x\n", table);
	lTable = table;
}



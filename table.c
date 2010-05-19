#include <assert.h>
#include "defs.h"
GTable* gTable;
LTable* lTable;
UInt32	maxRegionLevel;

UInt32 getTEntrySize() {
	return maxRegionLevel;
}


void freeTEntry(TEntry* entry);

GTable* allocGlobalTable(int depth) {
	GTable* ret = (GTable*) malloc(sizeof(GTable));
	bzero(ret->array, sizeof(GTable));
	return ret;
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


TEntry* allocTEntry(int size) {
	TEntry* ret = (TEntry*) malloc(sizeof(TEntry));
	ret->version = (UInt32*) malloc(sizeof(UInt32) * size);
	ret->time = (UInt64*) malloc(sizeof(UInt64) * size);
	bzero(ret->version, sizeof(UInt32) * size);
	bzero(ret->time, sizeof(UInt64) * size);
	return ret;
}

void freeTEntry(TEntry* entry) {
	free(entry->version);
	free(entry->time);
	free(entry);
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
	free(table->array);
	free(table);
}

void setLocalTable(LTable* table) {
//	printf("Set LTable to 0x%x\n", table);
	lTable = table;
}

void initDataStructure(int regionLevel) {
	maxRegionLevel = regionLevel;
	gTable = allocGlobalTable(maxRegionLevel);
}

void finalizeDataStructure() {
	freeGlobalTable(gTable);
}

void printTEntry(TEntry* entry) {
	int i;
	for (i = 0; i < getTEntrySize(); i++) {
		printf("%u %lld\n", entry->version[i], entry->time[i]);
	}
}

TEntry* getLTEntry(UInt32 vreg) {
	assert(lTable != NULL);
	if (vreg >= lTable->size) {
		fprintf(stderr,"vreg = %d, lTable size = %d\n", vreg, lTable->size);
	}
	assert(vreg < lTable->size);
	
	return lTable->array[vreg];	
}

// FIXME: 64bit address?
TEntry* getGTEntry(Addr addr) {
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
	}
	return ret;
}



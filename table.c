#include <assert.h>
#include "defs.h"
GTable* gTable;
LTable* lTable;
UInt32	maxRegionLevel;

UInt32 getMaxRegionLevel() {
	return maxRegionLevel;
}

void setMaxRegionLevel(UInt32 max) {
	maxRegionLevel = max;
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
	for (i=0; i<maxRegionLevel; i++) {
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
		ret->array[i] = allocTEntry(getMaxRegionLevel());
	}
	return ret;
}


void freeLocalTable(LTable* table) {
	int i;
	assert(table->array != NULL);
	for (i=0; i<table->size; i++) {
		assert(table->array[i] != NULL);
		freeTEntry(table->array[i]);
	}
	free(table->array);
	free(table);
}

void setLocalTable(LTable* table) {
	lTable = table;
}

void initDataStructure(int regionLevel) {
	gTable = allocGlobalTable(maxRegionLevel);
	maxRegionLevel = regionLevel;
}

void finalizeDataStructure() {
	freeGlobalTable(gTable);
}

#if 0
UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version) {
	UInt64 ret = (entry->version[level] == version) ?
					entry->time[level] : 0;
	return ret;
}

UInt64 getTimestampNoVersion(TEntry* entry, UInt32 level) {
	return entry->time[level];
}
#endif

void updateTimestamp(TEntry* entry, UInt32 level, UInt32 version, UInt64 timestamp) {
	entry->version[level] = version;
	entry->time[level] = timestamp;	
}

void printTEntry(TEntry* entry) {
	int i;
	for (i = 0; i < getMaxRegionLevel(); i++) {
		printf("%d %lld\n", entry->version[i], entry->time[i]);
	}
}

TEntry* getLTEntry(UInt32 vreg) {
	assert(lTable != NULL);
	return lTable->array[vreg];	
}

// FIXME: 64bit address?
TEntry* getGTEntry(Addr addr) {
	UInt32 index = ((UInt64) addr >> 16);
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

#if 0
void updateLocalTime(LTable* table, int key, UInt64 timestamp, UInt32 version) {
	assert(key < table->size);
	table->array[key]->time = timestamp;	
	table->array[key]->version = version;
}

UInt64 getLocalTime(LTable* table, int key, UInt32 version) {
	assert(key < table->size);
	TEntry* entry = table->array[key];
	UInt64 ret = entry->version[
	return table->array[key];
}
#endif


#if 0
static inline int getIndex(Addr addr) {
	return ((UInt64)addr >> 2) & (GTABLE_SIZE - 1);
}
#endif

#if 0
GTEntry* getGTEntry(GTable* table, Addr addr) {
	GTEntry* current = table->array[getIndex(addr)];
	while (current != NULL) {
		if (current->addr == addr)
			return current;
		current = current->next;
	}
	return NULL;
}

static GTEntry* createGTEntry(Addr addr) {
	GTEntry* ret = (GTEntry*) malloc(sizeof(GTEntry));
	ret->addr = addr;
	ret->time = 0;
	return ret;
}

static void insertGTEntry(GTable* table, GTEntry* entry) {
	int index = getIndex(entry->addr);
	entry->next = table->array[index];
	table->array[index] = entry;
}

UInt64 getGlobalTime(GTable* table, Addr addr) {
	GTEntry* entry = getGTEntry(table, addr);
	if (entry == NULL) {
		// lazy entry creation
		entry = createGTEntry(addr);
	}
	
	return entry->time;
}


void updateGlobalTime(GTable* table, Addr addr, UInt64 timestamp) {
	GTEntry* entry = getGTEntry(table, addr);
	if (entry == NULL) {
		// lazy entry creation
		entry = createGTEntry(addr);
		insertGTEntry(table, entry);
	}
	entry->time = timestamp;
}
#endif

#if 0
GTable* allocGlobalTable() {
	GTable* ret = (GTable*) malloc(sizeof(GTable));
	ret->entrySize = GTABLE_SIZE;
	bzero(ret->array, sizeof(GTEntry*) * GTABLE_SIZE);
	return ret;
}

void freeGlobalTable(GTable* table) {
	int i = 0;
	for (i = 0; i <	GTABLE_SIZE; i++) {
		GTEntry* entry = table->array[i];
		while (entry != NULL) {
			GTEntry* prev = entry;
			entry = entry->next;
			free(prev);
		}
	}
	free(table);
}
#endif

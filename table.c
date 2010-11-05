#include <assert.h>
#include "defs.h"
#include "table.h"
#include "kmalloc.h"

GTable* gTable;
LTable* lTable;
MTable* mTable;
UInt32	maxRegionLevel;

void dumpTableMemAlloc(void);

UInt32 getTEntrySize() {
	return maxRegionLevel;
}


void freeTEntry(TEntry* entry);

GTable* allocGlobalTable(int depth) {
	GTable* ret = (GTable*) kmalloc(sizeof(GTable));
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
		fprintf(stderr,"WARNING: %d entries left in kmalloc table. Should be 0.\n",table->size);
	}

	int i;
	for(i = 0; i < table->size; ++i) {
		kfree(table->array[i]);
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
			kfree(entry);
		}
	}
	kfree(table);
}

long long _tEntryLocalCnt = 0;
long long _tEntryGlobalCnt = 0;

TEntry* allocTEntry(int size) {
	TEntry* ret = (TEntry*) kmalloc(sizeof(TEntry));
	if (ret == NULL) {
		fprintf(stderr, "TEntry alloc error\n");
	}
	assert(ret != NULL);
	ret->version = (UInt32*) kmalloc(sizeof(UInt32) * size);
	ret->readVersion = (UInt32*) kmalloc(sizeof(UInt32) * size);
	ret->time = (UInt64*) kmalloc(sizeof(UInt64) * size);
	ret->readTime = (UInt64*) kmalloc(sizeof(UInt64) * size);

	if (ret->version == NULL || ret->time == NULL) {
		fprintf(stderr, "allocTEntry size = %d Each TEntry Size = %lu\n", 
			size, getTEntrySize());
		dumpTableMemAlloc(); 
		
		assert(kmalloc(sizeof(UInt64) * size) != NULL);
		fprintf(stderr, "additional kmalloc succeeded\n");
	}
	assert(ret->version != NULL && ret->time != NULL);
	
	bzero(ret->version, sizeof(UInt32) * size);
	bzero(ret->readVersion, sizeof(UInt32) * size);
	bzero(ret->time, sizeof(UInt64) * size);
	bzero(ret->readTime, sizeof(UInt64) * size);
	return ret;
}

void freeTEntry(TEntry* entry) {
	kfree(entry->version);
	kfree(entry->readVersion);
	kfree(entry->time);
	kfree(entry->readTime);
	kfree(entry);
}

void createMEntry(Addr start_addr, size_t entry_size) {
	MEntry* me = (MEntry*)kmalloc(sizeof(MEntry));

	me->start_addr = start_addr;
	me->size = entry_size;

	int mtable_size = mTable->size + 1;

	assert(mtable_size < MALLOC_TABLE_SIZE);
	mTable->size = mtable_size;

	mTable->array[mtable_size] = me;
}

void freeMEntry(Addr start_addr) {
	MEntry* me = NULL;

	// most likely to free something we recently kmalloced
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
	kfree(me);
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
	GEntry* ret = (GEntry*) kmalloc(sizeof(GEntry));
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
	LTable* ret = (LTable*) kmalloc(sizeof(LTable));
	ret->size = size;
	ret->array = (TEntry**) kmalloc(sizeof(TEntry*) * size);
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
	kfree(table->array);
	kfree(table);
}

void setLocalTable(LTable* table) {
//	printf("Set LTable to 0x%x\n", table);
	lTable = table;
}

void initDataStructure(int regionLevel) {
	fprintf(stderr, "# of instrumented region Levels = %d\n", regionLevel);
	maxRegionLevel = regionLevel;
	gTable = allocGlobalTable(maxRegionLevel);
	mTable = allocMallocTable();
}

void finalizeDataStructure() {
	freeGlobalTable(gTable);
	freeMallocTable(mTable);
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


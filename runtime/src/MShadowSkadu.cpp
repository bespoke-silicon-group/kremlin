#include <assert.h>
#include <stdio.h>

#include "kremlin.h"
#include "config.h"
#include "debug.h"
#include "MemMapAllocator.h"

#include "Table.h"
#include "CRegion.h"
#include "MShadowCache.h"
#include "MShadowSkadu.h"
#include "MShadowStat.h"
#include "compression.h"

#include <vector>

/*
 * SparseTable: sparse table that tracks 4GB memory chunks being used
 *
 * Since 64bit address is very sparsely used in a program,
 * we use a sparse table to reduce the memory requirement of the table.
 * Although the walk-through of a table might be pricey,
 * the use of cache will make the frequency of walk-through very low.
 */

#define STABLE_SIZE		32		// 128GB addr space will be enough

class SEntry {
public:
	UInt32 	addrHigh;	// upper 32bit in 64bit addr
	SegTable* segTable;
};

/*
 * SparseTable: introduced to support 64-bit address space
 *
 */

class SparseTable {
public:
	//SEntry entry[STABLE_SIZE];	
	std::vector<SEntry> entry;
	int writePtr;

	void init() { 
		entry.resize(STABLE_SIZE);
		writePtr = 0;
	}

	void deinit() {
		for (int i = 0; i < writePtr; i++) {
			SEntry* e = &entry[i];
			SegTable::Free(e->segTable);		
			eventSegTableFree();
		}
	}

	SEntry* getSEntry(Addr addr) {
		UInt32 highAddr = (UInt32)((UInt64)addr >> 32);

		// walk-through SparseTable
		for (int i=0; i < writePtr; i++) {
			if (entry[i].addrHigh == highAddr) {
				//MSG(0, "SparseTable Found an existing entry..\n");
				return &entry[i];	
			}
		}

		// not found - create an entry
		MSG(0, "SparseTable Creating a new Entry..\n");
		fprintf(stderr, "SparseTable Creating a new Entry..\n");

		SEntry* ret = &entry[writePtr];
		ret->addrHigh = highAddr;
		ret->segTable = SegTable::Alloc();
		eventSegTableAlloc();
		writePtr++;
		return ret;
	}
	
};

static SparseTable* sTable;


/*
 * Compression Configuration
 * instead of calling KConfigGetCompression repeatedly, 
 * store the result and reuse.
 */
static int _useCompression = 0;
static inline int useCompression() {
	return _useCompression;
}

static void setCompression() {
	_useCompression = KConfigGetCompression();
}



/*
 * TimeTable: simple array of Time with TIMETABLE_SIZE elements
 *
 */ 

TimeTable* TimeTable::Create(int size_type) {
	assert(size_type == TYPE_32BIT || size_type == TYPE_64BIT);
	int size = TimeTable::GetEntrySize(size_type);
	TimeTable* ret = (TimeTable*)MemPoolAllocSmall(sizeof(TimeTable));
	ret->array = (Time*)MemPoolAlloc();
	memset(ret->array, 0, sizeof(Time) * size);

	ret->type = size_type;
	ret->size = sizeof(Time) * TIMETABLE_SIZE / 2;

	eventTimeTableAlloc(size_type, ret->size);
	return ret;
}


void TimeTable::Destroy(TimeTable* table, UInt8 isCompressed) {
	eventTimeTableFree(table->type, table->size);
	int sizeType = table->type;
	assert(sizeType == TYPE_32BIT || sizeType == TYPE_64BIT);
	MemPoolFree(table->array);
	MemPoolFreeSmall(table, sizeof(TimeTable));
}

TimeTable* TimeTable::Create32BitClone(TimeTable* table) {
	assert(table->type == TYPE_64BIT);
	TimeTable* ret = TimeTable::Create(TYPE_32BIT);
	int i;
	for (i=0; i<TIMETABLE_SIZE/2; i++) {
		ret->array[i*2] = ret->array[i];
		ret->array[i*2 + 1] = ret->array[i];
	}
	return ret;
}


void TimeTable::setTimeAtAddr(Addr addr, Time time, UInt32 type) {
	int index = TimeTable::GetIndex(addr, this->type);

	MSG(3, "TimeTableSet to addr 0x%llx with index %d\n", &array[index], index);
	MSG(3, "\t table addr = 0x%llx, array addr = 0x%llx\n", this, &array[0]);

	array[index] = time;
	if (this->type == TYPE_32BIT && type == TYPE_64BIT) {
		array[index+1] = time;
	}
}


Time LevelTable::getTimeForAddrAtLevel(Index level, Addr addr, Version curr_ver) {
	TimeTable* table = this->getTimeTableAtLevel(level);
	Version stored_ver = this->vArray[level];

	Time ret = 0ULL;
	if (table == NULL) {
		ret = 0ULL;

	} else if (stored_ver == curr_ver) {
		ret = table->getTimeAtAddr(addr);
		MSG(0, "\t\tlv %d: \tversion = [%d, %d] value = %d\n", level, stored_ver, curr_ver, ret);
	} 

	return ret;
}

void LevelTable::setTimeForAddrAtLevel(Index level, Addr addr, Version curr_ver, Time value, UInt32 type) {
	TimeTable* table = this->getTimeTableAtLevel(level);
	Version stored_ver = this->getVersionAtLevel(level);
	eventLevelWrite(level);

	// no timeTable exists so create it
	if (table == NULL) {
		eventTimeTableNewAlloc(level, type);
		table = TimeTable::Create(type);

		this->setTimeTableAtLevel(level, table); 
		assert(table->type == type);
		table->setTimeAtAddr(addr, value, type);

	} else {
		// convert the table if needed
		if (type == TYPE_32BIT && table->type == TYPE_64BIT) {
			TimeTable* old = table;
			table = TimeTable::Create32BitClone(table);
			eventTimeTableConvertTo32();
			TimeTable::Destroy(old, this->isCompressed);
		}

		if (stored_ver == curr_ver) {
			assert(table != NULL);
			table->setTimeAtAddr(addr, value, type);

		} else {
			// exists but version is old so clean it and reuse
			table->clean();
			table->setTimeAtAddr(addr, value, type);
		}
	}
	this->setVersionAtLevel(level, curr_ver);
}


int LevelTable::findLowestInvalidIndex(Version* vArray) {
	int lowestInvalidIndex = 0;
	while(lowestInvalidIndex < MAX_LEVEL 
		&& this->tArray[lowestInvalidIndex] != NULL 
		&& this->vArray[lowestInvalidIndex] >= vArray[lowestInvalidIndex]) {
		++lowestInvalidIndex;
	}

	return lowestInvalidIndex;
}

void LevelTable::cleanTimeTablesFromLevel(Index start_level) {
	Index i;
	for(i = start_level; i < MAX_LEVEL; ++i) {
		TimeTable* time = this->tArray[i];
		if (time != NULL) {
			//fprintf(stderr, "(%d)\t", i);
			TimeTable::Destroy(time, this->isCompressed);
			this->tArray[i] = NULL;
		}
	}
}


/*
 * Garbage collection related logic
 */
static UInt64 nextGC = 1024;
static int gcPeriod = -1;

static void setGCPeriod(int time) {
	fprintf(stderr, "[kremlin] set GC period to %d\n", time);
	nextGC = time;
	gcPeriod = time;
	if (time == 0)
		nextGC = 0xFFFFFFFFFFFFFFFF;
}


static inline void gcLevelUnknownSize(LevelTable* lTable, Version* vArray) {
	int lii = lTable->findLowestInvalidIndex(vArray);
	lTable->cleanTimeTablesFromLevel(lii);
}

void gcLevel(LevelTable* table, Version* vArray, int size) {
	//fprintf(stderr, "%d: \t", size);
	int i;
	for (i=0; i<size; i++) {
		TimeTable* time = table->tArray[i];
		if (time == NULL)
			continue;

		Version ver = table->vArray[i];
		if (ver < vArray[i]) {
			// out of date
			//fprintf(stderr, "(%d, %d %d)\t", i, ver, vArray[i]);
			TimeTable::Destroy(time,table->isCompressed);
			table->tArray[i] = NULL;
		}
	}

	table->cleanTimeTablesFromLevel(size);
}

static void gcStart(Version* vArray, int size) {
	int i, j;
	eventGC();
	for (i=0; i<STABLE_SIZE; i++) {
		SegTable* table = sTable->entry[i].segTable;	
		if (table == NULL)
			continue;
		
		for (j=0; j<SEGTABLE_SIZE; j++) {
			LevelTable* lTable = table->entry[j];
			if (lTable != NULL) {
				gcLevel(lTable, vArray, size);
			}
		}
	}
}

/*
 * LevelTable operations
 */

// TODO: make this a method in SparseTable
static inline LevelTable* LevelTableGet(Addr addr, Version* vArray) {
	SEntry* sEntry = sTable->getSEntry(addr);
	SegTable* segTable = sEntry->segTable;
	assert(segTable != NULL);
	int segIndex = SegTable::GetIndex(addr);
	LevelTable* lTable = segTable->entry[segIndex];
	if (lTable == NULL) {
		lTable = LevelTable::Alloc();
		if (useCompression()) {
			int compressGain = CBufferAdd(lTable);
			eventCompression(compressGain);
		}
		segTable->entry[segIndex] = lTable;
		eventLevelTableAlloc();
	}
	
	if(useCompression() && lTable->isCompressed) {
		gcLevelUnknownSize(lTable,vArray);
		int gain = CBufferDecompress(lTable);
		eventCompression(gain);
	}


	return lTable;
}
static void check(Addr addr, Time* src, int size, int site) {
#ifndef NDEBUG
	int i;

	for (i=1; i<size; i++) {
		if (src[i-1] < src[i]) {
			fprintf(stderr, "site %d Addr %p size %d offset %d val=%ld %ld\n", 
				site, addr, size, i, src[i-1], src[i]); 
			assert(0);
		}
	}
#endif
}

static inline int hasVersionError(Version* vArray, int size) {
#ifndef NDEBUG
	int i;
	for (i=1; i<size; i++) {
		if (vArray[i-1] > vArray[i])
			return 1;
	}
#endif
	return 0;
}

/*
 * Fetch / Evict from TVCache to TVStorage
 */

void SkaduEvict(Time* tArray, Addr addr, int size, Version* vArray, int type) {
	if (addr == NULL)
		return;

	int i;
	//int startInvalid = getStartInvalidLevel(oldVersion, vArray, size);

	MSG(0, "\tTVCacheEvict 0x%llx, size=%d, effectiveSize=%d \n", addr, size, size);
		
	LevelTable* lTable = LevelTableGet(addr,vArray);
	for (i=0; i<size; i++) {
		eventEvict(i);
		if (tArray[i] == 0ULL) {
			break;
		}
		lTable->setTimeForAddrAtLevel(i, addr, vArray[i], tArray[i], type);
		MSG(0, "\t\toffset=%d, version=%d, value=%d\n", i, vArray[i], tArray[i]);
	}
	eventCacheEvict(size, size);

	//fprintf(stderr, "\tTVCacheEvict lTable=%llx, 0x%llx, size=%d, effectiveSize=%d \n", lTable, addr, size, startInvalid);
	if (useCompression())
		CBufferAccess(lTable);
	
	
	check(addr, tArray, size, 3);
}

void SkaduFetch(Addr addr, Index size, Version* vArray, Time* destAddr, int type) {
	MSG(0, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	//fprintf(stderr, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	LevelTable* lTable = LevelTableGet(addr, vArray);

	int i;
	for (i=0; i<size; i++) {
		destAddr[i] = lTable->getTimeForAddrAtLevel(i, addr, vArray[i]);
	}

	if (useCompression())
		CBufferAccess(lTable);
}


/*
 * Actual load / store handlers without TVCache
 */

static Time tempArray[1000];

static Time* NoCacheGet(Addr addr, Index size, Version* vArray, int type) {
	LevelTable* lTable = LevelTableGet(addr, vArray);	
	Index i;
	for (i=0; i<size; i++) {
		tempArray[i] = lTable->getTimeForAddrAtLevel(i, addr, vArray[i]);
	}

	if (useCompression())
		CBufferAccess(lTable);

	return tempArray;	
}

static void NoCacheSet(Addr addr, Index size, Version* vArray, Time* tArray, int type) {
	LevelTable* lTable = LevelTableGet(addr, vArray);	
	assert(lTable != NULL);
	Index i;
	for (i=0; i<size; i++) {
		lTable->setTimeForAddrAtLevel(i, addr, vArray[i], tArray[i], type);
	}

	if (useCompression())
		CBufferAccess(lTable);
}


/*
 * Entry point functions from Kremlin
 */

static Time* _MShadowSkaduGet(Addr addr, Index size, Version* vArray, UInt32 width) {
	if (size < 1)
		return NULL;

	//int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	int type = TYPE_64BIT; 

	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "MShadowGet 0x%llx, size %d \n", tAddr, size);
	eventRead();
	if (KConfigUseSkaduCache() == FALSE) {
		return NoCacheGet(tAddr, size, vArray, type);

	} else {
		return TVCacheGet(tAddr, size, vArray, type);
	}
}

static void _MShadowSkaduSet(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	MSG(0, "MShadowSet 0x%llx, size %d [", addr, size);
	if (size < 1)
		return;

	if (getActiveTimeTableSize() >= nextGC) {
		gcStart(vArray, size);
		//nextGC = stat.nTimeTableActive + gcPeriod;
		nextGC += gcPeriod;
	}

	//int type = (width > 4) ? TYPE_64BIT: TYPE_32BIT;
	int type = TYPE_64BIT;


	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "]\n");
	eventWrite();
	if (KConfigUseSkaduCache() == FALSE) {
		NoCacheSet(tAddr, size, vArray, tArray, type);

	} else {
		TVCacheSet(tAddr, size, vArray, tArray, type);
	}
}

/*
 * Init / Deinit
 */

void MShadowInitSkadu() {
	int cacheSizeMB = KConfigGetSkaduCacheSize();
	fprintf(stderr,"[kremlin] MShadow Init with cache %d MB, TimeTableSize = %ld\n",
		cacheSizeMB, sizeof(TimeTable));

	int size = TimeTable::GetEntrySize(TYPE_64BIT);
	MemPoolInit(1024, size * sizeof(Time));
	
	setGCPeriod(KConfigGetGCPeriod());
 
	sTable = new SparseTable();
	sTable->init();
	TVCacheInit(cacheSizeMB);

	CBufferInit(KConfigGetCBufferSize());
	MShadowGet = _MShadowSkaduGet;
	MShadowSet = _MShadowSkaduSet;
	setCompression();
}


void MShadowDeinitSkadu() {
	CBufferDeinit();
	MShadowStatPrint();
	sTable->deinit();
	TVCacheDeinit();
}




#include <assert.h>
#include <stdio.h>

#include "kremlin.h"
#include "config.h"
#include "debug.h"
#include "MemMapAllocator.h"

#include "Table.h"
#include "CRegion.h"
#include "MShadowSkadu.h"
#include "MShadowStat.h"
#include "compression.h"

#include "MShadowCache.h"
#include "MShadowNullCache.h"

#include <vector>

/*
 * SparseTable: sparse table that tracks 4GB memory chunks being used
 *
 * Since 64bit address is very sparsely used in a program,
 * we use a sparse table to reduce the memory requirement of the table.
 * Although the walk-through of a table might be pricey,
 * the use of cache will make the frequency of walk-through very low.
 */

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
	static const unsigned int NUM_ENTRIES = 32;

	std::vector<SEntry> entry;
	int writePtr;

	void init() { 
		entry.resize(SparseTable::NUM_ENTRIES);
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

		SEntry* ret = &entry[writePtr];
		ret->addrHigh = highAddr;
		ret->segTable = SegTable::Alloc();
		eventSegTableAlloc();
		writePtr++;
		return ret;
	}
	
};

static SparseTable* sTable; // TODO: make a private member of MShadowSkadu

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

TimeTable* TimeTable::Create(TimeTable::TableType size_type) {
	assert(size_type == TimeTable::TYPE_32BIT || size_type == TimeTable::TYPE_64BIT);
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
	assert(sizeType == TimeTable::TYPE_32BIT || sizeType == TimeTable::TYPE_64BIT);
	MemPoolFree(table->array);
	MemPoolFreeSmall(table, sizeof(TimeTable));
}

TimeTable* TimeTable::Create32BitClone(TimeTable* table) {
	assert(table->type == TimeTable::TYPE_64BIT);
	TimeTable* ret = TimeTable::Create(TimeTable::TYPE_32BIT);
	int i;
	for (i=0; i<TIMETABLE_SIZE/2; i++) {
		ret->array[i*2] = ret->array[i];
		ret->array[i*2 + 1] = ret->array[i];
	}
	return ret;
}


void TimeTable::setTimeAtAddr(Addr addr, Time time, TimeTable::TableType type) {
	int index = TimeTable::GetIndex(addr, this->type);

	MSG(3, "TimeTableSet to addr 0x%llx with index %d\n", &array[index], index);
	MSG(3, "\t table addr = 0x%llx, array addr = 0x%llx\n", this, &array[0]);

	array[index] = time;
	if (this->type == TimeTable::TYPE_32BIT && type == TimeTable::TYPE_64BIT) {
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

void LevelTable::setTimeForAddrAtLevel(Index level, Addr addr, Version curr_ver, Time value, TimeTable::TableType type) {
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
		if (type == TimeTable::TYPE_32BIT && table->type == TimeTable::TYPE_64BIT) {
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
	while(lowestInvalidIndex < LevelTable::MAX_LEVEL 
		&& this->tArray[lowestInvalidIndex] != NULL 
		&& this->vArray[lowestInvalidIndex] >= vArray[lowestInvalidIndex]) {
		++lowestInvalidIndex;
	}

	return lowestInvalidIndex;
}

void LevelTable::cleanTimeTablesFromLevel(Index start_level) {
	Index i;
	for(i = start_level; i < LevelTable::MAX_LEVEL; ++i) {
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
void MShadowSkadu::initGarbageCollector(int period) {
	fprintf(stderr, "[kremlin] set GC period to %d\n", period);
	next_gc_time = period;
	gc_period = period;
	if (period == 0) next_gc_time = 0xFFFFFFFFFFFFFFFF;
}

void MShadowSkadu::runGarbageCollector(Version* versions, int size) {
	eventGC();
	for (unsigned i = 0; i < SparseTable::NUM_ENTRIES; ++i) {
		SegTable* table = sTable->entry[i].segTable;	
		if (table == NULL)
			continue;
		
		for (unsigned j = 0; j < SegTable::SEGTABLE_SIZE; ++j) {
			LevelTable* lTable = table->entry[j];
			if (lTable != NULL) {
				lTable->gcLevel(versions, size);
			}
		}
	}
}

void LevelTable::gcLevel(Version* versions, int size) {
	//fprintf(stderr, "%d: \t", size);
	int i;
	for (i=0; i<size; i++) {
		TimeTable* time = this->tArray[i];
		if (time == NULL)
			continue;

		Version ver = this->vArray[i];
		if (ver < versions[i]) {
			// out of date
			//fprintf(stderr, "(%d, %d %d)\t", i, ver, versions[i]);
			TimeTable::Destroy(time,this->isCompressed);
			this->tArray[i] = NULL;
		}
	}

	this->cleanTimeTablesFromLevel(size);
}

void LevelTable::gcLevelUnknownSize(Version* versions) {
	int lii = this->findLowestInvalidIndex(versions);
	this->cleanTimeTablesFromLevel(lii);
}

/*
 * LevelTable operations
 */

LevelTable* MShadowSkadu::getLevelTable(Addr addr, Version* vArray) {
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
		lTable->gcLevelUnknownSize(vArray);
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

void MShadowSkadu::evict(Time* tArray, Addr addr, int size, Version* vArray, TimeTable::TableType type) {
	if (addr == NULL)
		return;


	MSG(0, "\tTVCacheEvict 0x%llx, size=%d, effectiveSize=%d \n", addr, size, size);
		
	LevelTable* lTable = this->getLevelTable(addr,vArray);
	for (int i=0; i<size; i++) {
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

void MShadowSkadu::fetch(Addr addr, Index size, Version* vArray, Time* destAddr, TimeTable::TableType type) {
	MSG(0, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	//fprintf(stderr, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	LevelTable* lTable = this->getLevelTable(addr, vArray);

	int i;
	for (i=0; i<size; i++) {
		destAddr[i] = lTable->getTimeForAddrAtLevel(i, addr, vArray[i]);
	}

	if (useCompression())
		CBufferAccess(lTable);
}


/*
 * Entry point functions from Kremlin
 */

Time* MShadowSkadu::get(Addr addr, Index size, Version* vArray, UInt32 width) {
	if (size < 1) return NULL;

	//TimeTable::TableType type = (width > 4) ? TimeTable::TYPE_64BIT: TimeTable::TYPE_32BIT;
	TimeTable::TableType type = TimeTable::TYPE_64BIT; 

	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "MShadowGet 0x%llx, size %d \n", tAddr, size);
	eventRead();
	return cache->get(tAddr, size, vArray, type);
}

void MShadowSkadu::set(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	MSG(0, "MShadowSet 0x%llx, size %d [", addr, size);
	if (size < 1)
		return;

	if (getActiveTimeTableSize() >= next_gc_time) {
		runGarbageCollector(vArray, size);
		//next_gc_time = stat.nTimeTableActive + gc_period;
		next_gc_time += gc_period;
	}

	//TimeTable::TableType type = (width > 4) ? TimeTable::TYPE_64BIT: TimeTable::TYPE_32BIT;
	TimeTable::TableType type = TimeTable::TYPE_64BIT;


	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "]\n");
	eventWrite();
	cache->set(tAddr, size, vArray, tArray, type);
}

/*
 * Init / Deinit
 */

void MShadowSkadu::init() {
	
	int cacheSizeMB = KConfigGetSkaduCacheSize();
	fprintf(stderr,"[kremlin] MShadow Init with cache %d MB, TimeTableSize = %ld\n",
		cacheSizeMB, sizeof(TimeTable));

	if (KConfigUseSkaduCache() == TRUE) 
		cache = new SkaduCache();
	else
		cache = new NullCache();

	cache->init(cacheSizeMB, KConfigGetCompression(), this);

	int size = TimeTable::GetEntrySize(TimeTable::TYPE_64BIT);
	MemPoolInit(1024, size * sizeof(Time));
	
	initGarbageCollector(KConfigGetGCPeriod());
 
	sTable = new SparseTable();
	sTable->init();

	CBufferInit(KConfigGetCBufferSize());
	setCompression();
}


void MShadowSkadu::deinit() {
	cache->deinit();
	delete cache;
	cache = NULL;
	CBufferDeinit();
	MShadowStatPrint();
	sTable->deinit();
}

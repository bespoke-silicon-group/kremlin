#include <cassert>
#include <string.h> // for memset
#include <vector>

#include "config.h"
#include "debug.h"
#include "MemMapAllocator.h"

#include "LevelTable.hpp"
#include "MemorySegment.hpp"
#include "MShadowSkadu.h"
#include "MShadowStat.h" // for event counters
#include "compression.h" // for CBuffer

#include "MShadowCache.h"
#include "MShadowNullCache.h"

/*!
 * @brief A sparse table that tracks 4GB memory chunks being used.
 *
 * Since 64bit address is very sparsely used in a program,
 * we use a sparse table to reduce the memory requirement of the table.
 * Although the walk-through of a table might be pricey,
 * the use of cache will make the frequency of walk-through very low.
 */
class SparseTableElement {
public:
	UInt32 	addrHigh;	// upper 32bit in 64bit addr
	MemorySegment* segTable;
};

/*!
 * @brief A class to efficiently support 64-bit address space
 */
class SparseTable {
public:
	static const unsigned int NUM_ENTRIES = 32;

	std::vector<SparseTableElement> entry;
	int writePtr;

	void init() { 
		entry.resize(SparseTable::NUM_ENTRIES);
		writePtr = 0;
	}

	void deinit() {
		for (int i = 0; i < writePtr; i++) {
			SparseTableElement* e = &entry[i];
			delete e->segTable;		
			e->segTable = NULL;
			eventSegTableFree();
		}
	}

	SparseTableElement* getElement(Addr addr) {
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

		SparseTableElement* ret = &entry[writePtr];
		ret->addrHigh = highAddr;
		ret->segTable = new MemorySegment();
		eventSegTableAlloc();
		writePtr++;
		return ret;
	}
	
};

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

void MShadowSkadu::initGarbageCollector(unsigned period) {
	MSG(3, "set garbage collection period to %u\n", period);
	next_gc_time = period;
	garbage_collection_period = period;
	if (period == 0) next_gc_time = 0xFFFFFFFFFFFFFFFF;
}

void MShadowSkadu::runGarbageCollector(Version* curr_versions, int size) {
	eventGC();
	for (unsigned i = 0; i < SparseTable::NUM_ENTRIES; ++i) {
		MemorySegment* table = sparse_table->entry[i].segTable;	
		if (table == NULL)
			continue;
		
		for (unsigned j = 0; j < MemorySegment::getNumLevelTables(); ++j) {
			LevelTable* lTable = table->getLevelTableAtIndex(j);
			if (lTable != NULL) {
				lTable->collectGarbageWithinBounds(curr_versions, size);
			}
		}
	}
}

LevelTable* MShadowSkadu::getLevelTable(Addr addr, Version* vArray) {
	SparseTableElement* sEntry = sparse_table->getElement(addr);
	MemorySegment* segTable = sEntry->segTable;
	assert(segTable != NULL);
	unsigned segIndex = MemorySegment::GetIndex(addr);
	LevelTable* lTable = segTable->getLevelTableAtIndex(segIndex);
	if (lTable == NULL) {
		lTable = new LevelTable();
		if (useCompression()) {
			int compressGain = compression_buffer->add(lTable);
			eventCompression(compressGain);
		}
		segTable->setLevelTableAtIndex(lTable, segIndex);
		eventLevelTableAlloc();
	}
	
	if(useCompression() && lTable->isCompressed()) {
		lTable->collectGarbageUnbounded(vArray);
		int gain = compression_buffer->decompress(lTable);
		eventCompression(gain);
	}


	return lTable;
}

static void check(Addr addr, Time* src, int size, int site) {
#ifndef NDEBUG
	int i;

	for (i=1; i<size; i++) {
		if (src[i-1] < src[i]) {
			MSG(4, "site %d Addr %p size %d offset %d val=%ld %ld\n", 
				site, addr, size, i, src[i-1], src[i]); 
			assert(0);
		}
	}
#endif
}

void* MemorySegment::operator new(size_t size) {
	return MemPoolAllocSmall(sizeof(MemorySegment));
}

void MemorySegment::operator delete(void* ptr) {
	MemPoolFreeSmall(ptr, sizeof(MemorySegment));
}

MemorySegment::MemorySegment() {
	memset(this->level_tables, 0, 
			MemorySegment::NUM_ENTRIES * sizeof(LevelTable*));
}

MemorySegment::~MemorySegment() {
	for (unsigned i = 0; i < MemorySegment::NUM_ENTRIES; ++i) {
		LevelTable *l = level_tables[i];
		if (l != NULL) {
			delete l;
			l = NULL;
		}
	}
}

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

	//MSG(3, "\tTVCacheEvict lTable=%llx, 0x%llx, size=%d, effectiveSize=%d \n", lTable, addr, size, startInvalid);
	if (useCompression())
		compression_buffer->touch(lTable);
	
	
	check(addr, tArray, size, 3);
}

void MShadowSkadu::fetch(Addr addr, Index size, Version* vArray, Time* destAddr, TimeTable::TableType type) {
	MSG(0, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	//MSG(3, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	LevelTable* lTable = this->getLevelTable(addr, vArray);

	int i;
	for (i=0; i<size; i++) {
		destAddr[i] = lTable->getTimeForAddrAtLevel(i, addr, vArray[i]);
	}

	if (useCompression())
		compression_buffer->touch(lTable);
}

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
		//next_gc_time = stat.nTimeTableActive + garbage_collection_period;
		next_gc_time += garbage_collection_period;
	}

	//TimeTable::TableType type = (width > 4) ? TimeTable::TYPE_64BIT: TimeTable::TYPE_32BIT;
	TimeTable::TableType type = TimeTable::TYPE_64BIT;


	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "]\n");
	eventWrite();
	cache->set(tAddr, size, vArray, tArray, type);
}

void MShadowSkadu::init() {
	
	int cacheSizeMB = KConfigGetSkaduCacheSize();
	MSG(1,"MShadow Init with cache %d MB, TimeTableSize = %ld\n",
		cacheSizeMB, sizeof(TimeTable));

	if (KConfigUseSkaduCache() == TRUE) 
		cache = new SkaduCache();
	else
		cache = new NullCache();

	cache->init(cacheSizeMB, KConfigGetCompression(), this);

	unsigned size = TimeTable::GetNumEntries(TimeTable::TYPE_64BIT);
	MemPoolInit(1024, size * sizeof(Time));
	
	initGarbageCollector(KConfigGetGCPeriod());
 
	sparse_table = new SparseTable();
	sparse_table->init();

	compression_buffer = new CBuffer();
	compression_buffer->init(KConfigGetCBufferSize());
	setCompression();
}


void MShadowSkadu::deinit() {
	cache->deinit();
	delete cache;
	cache = NULL;
	compression_buffer->deinit();
	delete compression_buffer;
	compression_buffer = NULL;
	MShadowStatPrint();
	sparse_table->deinit();
}

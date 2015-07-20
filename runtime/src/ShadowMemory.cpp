#include <cassert>
#include <string.h> // for memset
#include <vector>

#include "config.hpp"
#include "debug.hpp"
#include "MemMapAllocator.hpp"

#include "LevelTable.hpp"
#include "MemorySegment.hpp"
#include "ShadowMemory.hpp"
#include "MShadowStat.hpp" // for event counters
#include "compression.hpp" // for CBuffer

#include "ShadowMemoryCache.hpp"
#include "MShadowNullCache.hpp"

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
	uint32_t 	addrHigh;	// upper 32bit in 64bit addr
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

	SparseTable() { 
		entry.resize(SparseTable::NUM_ENTRIES);
		writePtr = 0;
	}

	~SparseTable() {
		for (int i = 0; i < writePtr; i++) {
			SparseTableElement* e = &entry[i];
			delete e->segTable;		
			e->segTable = nullptr;
			eventSegTableFree();
		}
	}

	SparseTableElement* getElement(Addr addr) {
		uint32_t highAddr = (uint32_t)((uint64_t)addr >> 32);

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

void ShadowMemory::runGarbageCollector(Version* curr_versions, int size) {
	eventGC();
	for (unsigned i = 0; i < SparseTable::NUM_ENTRIES; ++i) {
		MemorySegment* table = sparse_table->entry[i].segTable;	
		if (table == nullptr)
			continue;
		
		for (unsigned j = 0; j < MemorySegment::getNumLevelTables(); ++j) {
			LevelTable* lTable = table->getLevelTableAtIndex(j);
			if (lTable != nullptr) {
				lTable->collectGarbageWithinBounds(curr_versions, size);
			}
		}
	}
}

LevelTable* ShadowMemory::getLevelTable(Addr addr, Version *curr_versions) {
	assert(curr_versions != nullptr);

	SparseTableElement* sEntry = sparse_table->getElement(addr);
	MemorySegment* segTable = sEntry->segTable;
	assert(segTable != nullptr);
	unsigned segIndex = MemorySegment::GetIndex(addr);
	LevelTable* lTable = segTable->getLevelTableAtIndex(segIndex);
	if (lTable == nullptr) {
		lTable = new LevelTable();
		if (useCompression()) {
			int compressGain = compression_buffer->add(lTable);
			eventCompression(compressGain);
		}
		segTable->setLevelTableAtIndex(lTable, segIndex);
		eventLevelTableAlloc();
	}
	
	if(useCompression() && lTable->isCompressed()) {
		lTable->collectGarbageUnbounded(curr_versions);
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
		if (l != nullptr) {
			delete l;
			l = nullptr;
		}
	}
}

void ShadowMemory::evict(Time *new_timestamps, Addr addr, Index size, Version *curr_versions, TimeTable::TableType type) {
	assert(new_timestamps != nullptr);
	assert(curr_versions != nullptr);

	MSG(0, "\tmshadow evict 0x%llx, size=%u, effectiveSize=%u \n", addr, size, size);
		
	LevelTable* lTable = this->getLevelTable(addr,curr_versions);
	for (unsigned i = 0; i < size; ++i) {
		eventEvict(i);
		if (new_timestamps[i] == 0ULL) { break; }
		lTable->setTimeForAddrAtLevel(i, addr, curr_versions[i], 
										new_timestamps[i], type);
		MSG(0, "\t\toffset=%u, version=%llu, value=%llu\n", 
			i, curr_versions[i], new_timestamps[i]);
	}
	eventCacheEvict(size, size);

	if (useCompression())
		compression_buffer->touch(lTable);
	
	check(addr, new_timestamps, size, 3);
}

void ShadowMemory::fetch(Addr addr, Index size, Version *curr_versions, 
							Time *timestamps, TimeTable::TableType type) {
	assert(curr_versions != nullptr);
	assert(timestamps != nullptr);

	MSG(3, "\tmshadow fetch 0x%llx, size %u\n", addr, size);
	LevelTable* lTable = this->getLevelTable(addr, curr_versions);

	for (Index i = 0; i < size; ++i) {
		timestamps[i] = lTable->getTimeForAddrAtLevel(i, addr, 
															curr_versions[i]);
	}

	if (useCompression())
		compression_buffer->touch(lTable);
}

Time* ShadowMemory::get(Addr addr, Index size, Version *curr_versions, 
						uint32_t width) {
	assert(curr_versions != nullptr);

	if (size < 1) return nullptr;

	TimeTable::TableType type = TimeTable::TYPE_64BIT; // FIXME: assumes 64 bit

	Addr tAddr = (Addr)((uint64_t)addr & ~(uint64_t)0x7);
	MSG(0, "mshadow get 0x%llx, size %u \n", tAddr, size);
	eventRead();

	return cache->get(tAddr, size, curr_versions, type);
}

void ShadowMemory::set(Addr addr, Index size, Version *curr_versions, 
						Time *timestamps, uint32_t width) {
	assert(curr_versions != nullptr);
	assert(timestamps != nullptr);
	
	MSG(0, "mshadow set 0x%llx, size %u [", addr, size);
	if (size < 1) return;

	if (getActiveTimeTableSize() >= next_gc_time) {
		runGarbageCollector(curr_versions, size);
		//next_gc_time = stat.nTimeTableActive + garbage_collection_period;
		next_gc_time += garbage_collection_period;
	}

	//TimeTable::TableType type = (width > 4) ? TimeTable::TYPE_64BIT: TimeTable::TYPE_32BIT;
	TimeTable::TableType type = TimeTable::TYPE_64BIT;


	Addr tAddr = (Addr)((uint64_t)addr & ~(uint64_t)0x7);
	MSG(0, "]\n");
	eventWrite();
	cache->set(tAddr, size, curr_versions, timestamps, type);
}

ShadowMemory::ShadowMemory(unsigned gc_period, bool enable_compress) 
	: sparse_table(std::unique_ptr<SparseTable>(new SparseTable())) 
	, next_gc_time(gc_period == 0 ? 0xFFFFFFFFFFFFFFFF : gc_period)
	, garbage_collection_period(gc_period)
	, compression_enabled(enable_compress)
	, compression_buffer(new CBuffer(kremlin_config.getNumCompressionBufferEntries())) {

	int cacheSizeMB = kremlin_config.getShadowMemCacheSizeInMB();
	MSG(1,"MShadow Init with cache %d MB, TimeTableSize = %ld\n",
		cacheSizeMB, sizeof(TimeTable));

	if (cacheSizeMB > 0) 
		cache = std::unique_ptr<CacheInterface>(new ShadowMemoryCache(cacheSizeMB, kremlin_config.compressShadowMem(), this));
	else
		cache = std::unique_ptr<CacheInterface>(new NullCache(cacheSizeMB, kremlin_config.compressShadowMem(), this));

	unsigned size = TimeTable::GetNumEntries(TimeTable::TYPE_64BIT);
	MemPoolInit(1024, size * sizeof(Time));
}

ShadowMemory::~ShadowMemory() {
	delete compression_buffer;
	compression_buffer = nullptr;
	MShadowStatPrint();
}

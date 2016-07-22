#ifndef _SHADOW_MEMORY_HPP
#define _SHADOW_MEMORY_HPP

#include <memory>
#include <cassert>
#include "ktypes.h"
#include "TimeTable.hpp" // for TimeTable::TableType

class SparseTable;
class MemorySegment;
class LevelTable;
class CacheInterface;
class CBuffer;

class ShadowMemory {
private:
	const std::unique_ptr<SparseTable> sparse_table; 

	uint64_t next_gc_time;
	unsigned garbage_collection_period;

	void runGarbageCollector(const Version * const curr_versions, int size);

	// TODO: make cache a const unique_ptr
	std::unique_ptr<CacheInterface> cache; //!< The cache associated with shadow mem

	bool compression_enabled; //!< Indicates whether we should use compression
	std::unique_ptr<CBuffer> compression_buffer;

	bool useCompression() {
		return compression_enabled;
	}

public:
	ShadowMemory() = delete;
	ShadowMemory(unsigned gc_period, bool enable_compress);
	~ShadowMemory(); // TODO: work on removing this so we can follow Rule of 0

	/*!
	 * @pre curr_versions is non-nullptr.
	 */
	Time* get(Addr addr, Index size, const Version * const curr_versions, 
				uint32_t width);

	void set(Addr addr, Index size, Version *curr_versions, 
				Time *timestamps, uint32_t width);

	CBuffer* getCompressionBuffer() { return compression_buffer.get(); }

	/*!
	 * @pre curr_versions and timestamps are non-nullptr.
	 */
	void fetch(Addr addr, Index size, const Version * constcurr_versions, 
				Time *timestamps, TimeTable::TableType type);

	/*!
	 * @pre new_timestamps and curr_versions are non-nullptr.
	 */
	void evict(Time *new_timestamps, Addr addr, Index size, 
				const Version * const curr_versions, TimeTable::TableType type);

	/*!
	 * @pre curr_versions is non-nullptr.
	 */
	LevelTable* getLevelTable(Addr addr, const Version * const curr_versions);
};

#endif

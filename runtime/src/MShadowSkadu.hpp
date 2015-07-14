#ifndef _MSHADOW_SKADU_H
#define _MSHADOW_SKADU_H

#include <memory>
#include <cassert>
#include "ktypes.h"
#include "MShadow.hpp" // for MShadow class 
#include "TimeTable.hpp" // for TimeTable::TableType

class SparseTable;
class MemorySegment;
class LevelTable;
class CacheInterface;
class CBuffer;

class MShadowSkadu : public MShadow {
private:
	const std::unique_ptr<SparseTable> sparse_table; 

	uint64_t next_gc_time;
	unsigned garbage_collection_period;

	void runGarbageCollector(Version *curr_versions, int size);

	// TODO: make cache a const unique_ptr
	std::unique_ptr<CacheInterface> cache; //!< The cache associated with shadow mem

	bool compression_enabled; //!< Indicates whether we should use compression
	CBuffer *compression_buffer;

	bool useCompression() {
		return compression_enabled;
	}

public:
	void init();
	void deinit();

	MShadowSkadu() = delete;
	MShadowSkadu(unsigned gc_period, bool enable_compress);
	~MShadowSkadu(); // TODO: work on removing this so we can follow Rule of 0

	/*!
	 * @pre curr_versions is non-nullptr.
	 */
	Time* get(Addr addr, Index size, Version *curr_versions, uint32_t width);

	void set(Addr addr, Index size, Version *curr_versions, 
				Time *timestamps, uint32_t width);

	CBuffer* getCompressionBuffer() { return compression_buffer; }

	/*!
	 * @pre curr_versions and timestamps are non-nullptr.
	 */
	void fetch(Addr addr, Index size, Version *curr_versions, 
				Time *timestamps, TimeTable::TableType type);

	/*!
	 * @pre new_timestamps and curr_versions are non-nullptr.
	 */
	void evict(Time *new_timestamps, Addr addr, Index size, 
				Version *curr_versions, TimeTable::TableType type);

	/*!
	 * @pre curr_versions is non-nullptr.
	 */
	LevelTable* getLevelTable(Addr addr, Version *curr_versions);
};

#endif

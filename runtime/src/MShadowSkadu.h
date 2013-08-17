#ifndef _MSHADOW_SKADU_H
#define _MSHADOW_SKADU_H

#include <cassert>
#include "ktypes.h"
#include "MShadow.h" // for MShadow class 
#include "TimeTable.hpp" // for TimeTable::TableType

class SparseTable;
class MemorySegment;
class LevelTable;
class CacheInterface;
class CBuffer;

class MShadowSkadu : public MShadow {
private:
	SparseTable *sparse_table; 

	UInt64 next_gc_time;
	unsigned garbage_collection_period;

	void initGarbageCollector(unsigned period);
	void runGarbageCollector(Version *curr_versions, int size);

	CacheInterface *cache; //!< The cache associated with shadow mem

	bool compression_enabled; //!< Indicates whether we should use compression
	CBuffer *compression_buffer;

	bool useCompression() {
		return compression_enabled;
	}

public:
	void init();
	void deinit();

	/*!
	 * @pre curr_versions is non-NULL.
	 */
	Time* get(Addr addr, Index size, Version *curr_versions, UInt32 width);

	void set(Addr addr, Index size, Version *curr_versions, 
				Time *timestamps, UInt32 width);

	CBuffer* getCompressionBuffer() { return compression_buffer; }

	/*!
	 * @pre curr_versions and timestamps are non-NULL.
	 */
	void fetch(Addr addr, Index size, Version *curr_versions, 
				Time *timestamps, TimeTable::TableType type);

	/*!
	 * @pre new_timestamps and curr_versions are non-NULL.
	 */
	void evict(Time *new_timestamps, Addr addr, Index size, 
				Version *curr_versions, TimeTable::TableType type);

	/*!
	 * @pre curr_versions is non-NULL.
	 */
	LevelTable* getLevelTable(Addr addr, Version *curr_versions);
};

#endif

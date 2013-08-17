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
	SparseTable* sparse_table; 

	UInt64 next_gc_time;
	unsigned garbage_collection_period;

	void initGarbageCollector(unsigned period);
	void runGarbageCollector(Version *curr_versions, int size);

	CacheInterface *cache;
	CBuffer* compression_buffer;

public:
	void init();
	void deinit();

	Time* get(Addr addr, Index size, Version* versions, UInt32 width);
	void set(Addr addr, Index size, Version* versions, Time* times, UInt32 width);

	void fetch(Addr addr, Index size, Version* vArray, Time* destAddr, TimeTable::TableType type);
	void evict(Time* tArray, Addr addr, int size, Version* vArray, TimeTable::TableType type);

	LevelTable* getLevelTable(Addr addr, Version* vArray);
	CBuffer* getCompressionBuffer() { return compression_buffer; }
};

#endif

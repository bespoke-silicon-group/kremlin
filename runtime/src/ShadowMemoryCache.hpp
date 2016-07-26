#ifndef __SHADOW_MEMORY_CACHE_HPP__
#define __SHADOW_MEMORY_CACHE_HPP__

#include "ktypes.h"
#include "CacheInterface.hpp"

class TagVectorCache;

class ShadowMemoryCache : public CacheInterface {
public:
	ShadowMemoryCache() = delete;
	ShadowMemoryCache(int size_in_mb, bool compress, ShadowMemory *mshadow);

	void  set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type);
	Time* get(Addr addr, Index size, const Version * const vArray, TimeTable::TableType type);

private:
	std::unique_ptr<TagVectorCache> tag_vector_cache;

	void evict(int index, const Version * const vArray);
	void flush(const Version * const vArray);
	void resize(int newSize, const Version * const vArray);
	void verifyVersionCapacity(int size, const Version * const vArray);
};

#endif

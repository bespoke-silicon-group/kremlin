#ifndef __SHADOW_MEMORY_CACHE_HPP__
#define __SHADOW_MEMORY_CACHE_HPP__

#include "ktypes.h"
#include "CacheInterface.hpp"

class TagVectorCache;

class ShadowMemoryCache : public CacheInterface {
public:
	ShadowMemoryCache() = delete;
	ShadowMemoryCache(int size_in_mb, bool compress, ShadowMemory *mshadow);
	~ShadowMemoryCache();

	void  set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type);
	Time* get(Addr addr, Index size, Version* vArray, TimeTable::TableType type);

private:
	TagVectorCache *tag_vector_cache;

	void evict(int index, Version* vArray);
	void flush(Version* vArray);
	void resize(int newSize, Version* vArray);
	void checkResize(int size, Version* vArray);
};

#endif

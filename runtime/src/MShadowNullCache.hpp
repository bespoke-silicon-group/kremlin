#ifndef MSHADOW_NULLCACHE_H
#define MSHADOW_NULLCACHE_H

#include "ktypes.h"
#include "CacheInterface.hpp"

/*
 * Actual load / store handlers without TVCache
 */

class NullCache : public CacheInterface {
public:
	NullCache() = delete;
	NullCache(int size, bool compress, ShadowMemory *mshadow) {
		this->use_compression = compress; 
		this->mem_shadow = mshadow;
	}
	~NullCache() {
		this->mem_shadow = nullptr;
	}

	void  set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type);
	Time* get(Addr addr, Index size, Version* vArray, TimeTable::TableType type);
};

#endif

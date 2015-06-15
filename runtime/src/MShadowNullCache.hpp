#ifndef MSHADOW_NULLCACHE_H
#define MSHADOW_NULLCACHE_H

#include "ktypes.h"
#include "CacheInterface.hpp"

/*
 * Actual load / store handlers without TVCache
 */

class NullCache : public CacheInterface {
public:
	void init(int size, bool compress, MShadowSkadu* mshadow) { 
		this->use_compression = compress;
		this->mem_shadow = mshadow;
	}
	void deinit() { this->mem_shadow = NULL; }

	void  set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type);
	Time* get(Addr addr, Index size, Version* vArray, TimeTable::TableType type);
};

#endif

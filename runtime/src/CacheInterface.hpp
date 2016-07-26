#ifndef _CACHEINTERFACE_HPP_
#define _CACHEINTERFACE_HPP_

#include "TimeTable.hpp" // for TimeTable::TableType

class ShadowMemory;

class CacheInterface {
protected:
	bool use_compression;
	ShadowMemory *mem_shadow;

public:
	CacheInterface(bool compress, ShadowMemory *shadow) : 
		use_compression(compress), mem_shadow(shadow) {}
	virtual ~CacheInterface() {}

	virtual void set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type) = 0;
	virtual Time* get(Addr addr, Index size, const Version * const vArray, TimeTable::TableType type) = 0;
};

#endif // _CACHEINTERFACE_HPP_

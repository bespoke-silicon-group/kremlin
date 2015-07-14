#ifndef _CACHEINTERFACE_HPP_
#define _CACHEINTERFACE_HPP_

#include "TimeTable.hpp" // for TimeTable::TableType

class MShadowSkadu;

class CacheInterface {
protected:
	bool use_compression;
	MShadowSkadu *mem_shadow;

public:
	virtual ~CacheInterface() {}

	virtual void set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type) = 0;
	virtual Time* get(Addr addr, Index size, Version* vArray, TimeTable::TableType type) = 0;
};

#endif // _CACHEINTERFACE_HPP_

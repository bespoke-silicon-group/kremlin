#ifndef MSHADOW_SKADUCACHE_H
#define MSHADOW_SKADUCACHE_H

#include "ktypes.h"
#include "CacheInterface.hpp"

class TagVectorCache;

class SkaduCache : public CacheInterface {
public:
	void init(int size, bool compress, MShadowSkadu* mshadow);
	void deinit();

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

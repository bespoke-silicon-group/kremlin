#include "kremlin.h"

#include <cassert>
#include <stdio.h>
#include <string.h> // for memcpy

#include "debug.h"
#include "config.h"
#include "MemMapAllocator.h"
#include "Table.h"
#include "compression.h"

#include "MShadowStat.h"
#include "MShadowSkadu.h"
#include "MShadowCache.h"
#include "TagVectorCache.h"
#include "TagVectorCacheLine.h"

//#define TVCacheDebug	0
static const int SKADU_CACHE_DEBUG_LVL = 0;

void SkaduCache::init(int size_in_mb, bool compress, MShadowSkadu *mshadow) {
	tag_vector_cache = new TagVectorCache();
	if (size_in_mb == 0) {
		MSG(0, "MShadowCache: Bypassing Cache\n"); 
	} else {
		tag_vector_cache->configure(size_in_mb, kremlin_config.getNumProfiledLevels());
	}
	this->use_compression = compress;
	this->mem_shadow = mshadow;
}

void SkaduCache::deinit() {
	if (kremlin_config.getShadowMemCacheSizeInMB() > 0) {
		// XXX: not sure of logic behind the next two lines (-sat)
		MemPoolFreeSmall(tag_vector_cache->tagTable, sizeof(TagVectorCacheLine) * tag_vector_cache->getLineCount());
		delete tag_vector_cache->valueTable;
	}
	delete tag_vector_cache;
	tag_vector_cache = NULL;
}

// XXX: not sure this should be a global function
int getStartInvalidLevel(Version lastVer, Version* vArray, Index size) {
	int firstInvalid = 0;
	if (size == 0)
		return 0;

	if (size > 2)
		MSG(SKADU_CACHE_DEBUG_LVL, "\tgetStartInvalidLevel lastVer = %lld, newVer = %lld %lld \n", 
			lastVer, vArray[size-2], vArray[size-1]);

	if (lastVer == vArray[size-1])
		return size;

	int i;
	for (i=size-1; i>=0; i--) {
		if (lastVer >= vArray[i]) {
			firstInvalid = i+1;
			break;
		}
	}
	return firstInvalid;

}
/*
 * TagVectorCache Evict / Flush / Resize 
 */

void SkaduCache::evict(int index, Version* vArray) {
	TagVectorCacheLine* line = tag_vector_cache->getTag(index);
	Addr addr = line->tag;
	if (addr == 0x0)
		return;

	int lastSize = line->lastSize[0];
	int lastVer = line->version[0];
	int evictSize = getStartInvalidLevel(lastVer, vArray, lastSize);
	Time* tArray0 = tag_vector_cache->getData(index, 0);
	mem_shadow->evict(tArray0, line->tag, evictSize, vArray, line->type);

	if (line->type == TimeTable::TYPE_32BIT) {
		lastSize = line->lastSize[1];
		lastVer = line->version[1];
		evictSize = getStartInvalidLevel(lastVer, vArray, lastSize);
		Time* tArray1 = tag_vector_cache->getData(index, 1);
		mem_shadow->evict(tArray1, (char*)line->tag+4, evictSize, vArray, TimeTable::TYPE_32BIT);
	}
}

void SkaduCache::flush(Version* vArray) {
	int i;
	int size = tag_vector_cache->getLineCount();
	for (i=0; i<size; i++) {
		evict(i, vArray);
	}
		
}

void SkaduCache::resize(int newSize, Version* vArray) {
	flush(vArray);
	int size = tag_vector_cache->getSize();
	int oldDepth = tag_vector_cache->getDepth();
	int newDepth = oldDepth + 10;

	MSG(SKADU_CACHE_DEBUG_LVL, "TVCacheResize from %d to %d\n", oldDepth, newDepth);
	MSG(SKADU_CACHE_DEBUG_LVL, "TVCacheResize from %d to %d\n", oldDepth, newDepth);
	tag_vector_cache->configure(size, newDepth);
}

void SkaduCache::checkResize(int size, Version* vArray) {
	int oldDepth = tag_vector_cache->getDepth();
	if (oldDepth < size) {
		resize(oldDepth + 10, vArray);
	}
}

static void check(Addr addr, Time* src, int size, int site) {
#ifndef NDEBUG
	int i;
	for (i=1; i<size; i++) {
		if (src[i-1] < src[i]) {
			fprintf(stderr, "site %d Addr %p size %d offset %d val=%ld %ld\n", 
				site, addr, size, i, src[i-1], src[i]); 
			assert(0);
		}
	}
#endif
}

Time* SkaduCache::get(Addr addr, Index size, Version* vArray, TimeTable::TableType type) {
	checkResize(size, vArray);
	TagVectorCacheLine* entry = NULL;
	Time* destAddr = NULL;
	int offset = 0;
	int index = 0;
	tag_vector_cache->lookupRead(addr, type, &index, &entry, &offset, &destAddr);
	check(addr, destAddr, entry->lastSize[offset], 0);

	if (entry->isHit(addr)) {
		eventReadHit();
		MSG(SKADU_CACHE_DEBUG_LVL, "\t cache hit at 0x%llx size = %d\n", destAddr, size);
		entry->validateTag(destAddr, vArray, size);
		check(addr, destAddr, size, 1);

	} else {
		// Unfortunately, this access results in a miss
		// 1. evict a line	
		eventReadEvict();
		evict(index, vArray);
#if 0
		Version lastVer = entry->version[offset];

		int lastSize = entry->lastSize[offset];
		int testSize = (size < lastSize) ? size : lastSize;
		int evictSize = getStartInvalidLevel(lastVer, vArray, testSize);

		MSG(0, "\t CacheGet: evict size = %d, lastSize = %d, size = %d\n", 
			evictSize, entry->lastSize[offset], size);
#endif

		// 2. read line from MShadow to the evicted line
		mem_shadow->fetch(addr, size, vArray, destAddr, type);
		entry->tag = addr;
		check(addr, destAddr, size, 2);
	}

	entry->setVersion(offset, vArray[size-1]);
	entry->setValidSize(offset, size);

	check(addr, destAddr, size, 3);
	return destAddr;
}

void SkaduCache::set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type) {
	checkResize(size, vArray);
	TagVectorCacheLine* entry = NULL;
	Time* destAddr = NULL;
	int index = 0;
	int offset = 0;

	tag_vector_cache->lookupWrite(addr, type, &index, &entry, &offset, &destAddr);
#if 0
#ifndef NDEBUG
	if (hasVersionError(vArray, size)) {
		assert(0);
	}
#endif
#endif

	if (entry->isHit(addr)) {
		eventWriteHit();
	} else {
		eventWriteEvict();
		evict(index, vArray);
#if 0
		Version lastVer = entry->version[offset];
		int lastSize = entry->lastSize[offset];
		int testSize = (size < lastSize) ? size : lastSize;
		int evictSize = getStartInvalidLevel(lastVer, vArray, testSize);

		//int evictSize = entry->lastSize[offset];
		//if (size < evictSize)
		//	evictSize = size;
		MSG(0, "\t CacheSet: evict size = %d, lastSize = %d, size = %d\n", 
			evictSize, lastSize, size);
		if (entry->tag != NULL)
			mem_shadow->evict(destAddr, entry->tag, evictSize, vArray, entry->type);
#endif
	} 		

	// copy Timestamps
	memcpy(destAddr, tArray, sizeof(Time) * size);
	if (entry->type == TimeTable::TYPE_32BIT && type == TimeTable::TYPE_64BIT) {
		// corner case: duplicate the timestamp
		// not yet implemented
		Time* duplicated = tag_vector_cache->getData(index, offset);
		memcpy(duplicated, tArray, sizeof(Time) * size);
	}
	entry->type = type;
	entry->tag = addr;
	entry->setVersion(offset, vArray[size-1]);
	entry->setValidSize(offset, size);

	check(addr, destAddr, size, 2);
}

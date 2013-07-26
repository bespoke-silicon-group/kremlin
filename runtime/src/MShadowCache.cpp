#include "kremlin.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "MemMapAllocator.h"
#include "debug.h"
#include "CRegion.h"
#include "MShadowSkadu.h"
#include "MShadowCache.h"
#include "MShadowStat.h"
#include "Table.h"
#include "compression.h"
#include "config.h"

#include <string.h> // for memcpy

#define TVCacheDebug	0

/*
 * TVCache: cache for tag vectors
 */ 

class CacheLine {
public:
	Addr tag;  
	Version version[2];
	int lastSize[2];	// required to know the region depth at eviction
	TimeTable::TableType type;

	Version getVersion(int offset) { return this->version[offset]; }

	void setVersion(int offset, Version ver) {
		//int index = lineIndex >> CACHE_VERSION_SHIFT;
		//verTable[lineIndex] = ver;
		this->version[offset] = ver;
	}

	void setValidSize(int offset, int size) {
		this->lastSize[offset] = size;
	}

	bool isHit(Addr addr) {
		// XXX: tag printed twice??? (-sat)
		MSG(3, "isHit addr = 0x%llx, tag = 0x%llx, entry tag = 0x%llx\n",
			addr, this->tag, this->tag);

		return (((UInt64)this->tag ^ (UInt64)addr) >> 3) == 0;
	}

	void print() {
		fprintf(stderr, "addr 0x%llx, ver [%d, %d], lastSize [%d, %d], type %d\n",
			this->tag, this->version[0], this->version[1], this->lastSize[0], this->lastSize[1], this->type);
	}

	void validateTag(Time* destAddr, Version* vArray, Index size);
};

class TVCache {
private:
	int  size_in_mb;
	int  line_count;
	int  line_shift;
	int  depth;

public:
	CacheLine* tagTable;
	Table* valueTable;

	int getSize() { return size_in_mb; }
	int getLineCount() { return line_count; }
	int getLineMask() { return line_count - 1; }
	int getDepth() { return depth; }
	int getLineShift() { return line_shift; }

	CacheLine* getTag(int index) {
		assert(index < getLineCount());
		return &tagTable[index];
	}

	Time* getData(int index, int offset) {
		return TableGetElementAddr(valueTable, index*2 + offset, 0);
	}

	int getLineIndex(Addr addr);

	void configure(int size_in_mb, int depth);
	void lookupRead(Addr addr, int type, int* pIndex, CacheLine** pLine, int* pOffset, Time** pTArray);
	void lookupWrite(Addr addr, int type, int *pIndex, CacheLine** pLine, int* pOffset, Time** pTArray);
};

static TVCache tag_vector_cache;

static inline int getFirstOnePosition(int input) {
	int i;

	for (i=0; i<8 * sizeof(int); i++) {
		if (input & (1 << i))
			return i;
	}
	assert(0);
	return 0;
}


void TVCache::configure(int new_size_in_mb, int new_depth) {
	const int new_line_size = 8;
	int new_line_count = new_size_in_mb * 1024 * 1024 / new_line_size;
	this->size_in_mb = new_size_in_mb;
	this->line_count = new_line_count;
	this->line_shift = getFirstOnePosition(new_line_count);
	this->depth = new_depth;

	fprintf(stderr, "MShadowCacheInit: size: %d MB, lineNum %d, lineShift %d, depth %d\n", 
		new_size_in_mb, new_line_count, this->line_shift, this->depth);

	tagTable = (CacheLine*)MemPoolCallocSmall(new_line_count, sizeof(CacheLine)); // 64bit granularity
	valueTable = TableCreate(new_line_count * 2, this->depth);  // 32bit granularity

	MSG(TVCacheDebug, "MShadowCacheInit: value Table created row %d col %d at addr 0x%x\n", 
		new_line_count, KConfigGetIndexSize(), valueTable[0].array);
}

int TVCache::getLineIndex(Addr addr) {
#if 0
	int nShift = 3; 	// 8 byte 
	int ret = (((UInt64)addr) >> nShift) & lineMask;
	assert(ret >= 0 && ret < lineNum);
#endif
	int nShift = 3;	
	int lineMask = getLineMask();
	int lineShift = getLineShift();
	int val0 = (((UInt64)addr) >> nShift) & lineMask;
	int val1 = (((UInt64)addr) >> (nShift + lineShift)) & lineMask;
	return val0 ^ val1;
}



void TVCache::lookupRead(Addr addr, int type, int* pIndex, CacheLine** pLine, int* pOffset, Time** pTArray) {
	int index = this->getLineIndex(addr);
	int offset = 0; 
	CacheLine* line = this->getTag(index);
	if (line->type == TimeTable::TYPE_32BIT && type == TimeTable::TYPE_64BIT) {
		// in this case, use the more recently one
		Time* option0 = this->getData(index, 0);
		Time* option1 = this->getData(index, 1);
		// check the first item only
		offset = (*option0 > *option1) ? 0 : 1;

	} else {
		offset = ((UInt64)addr >> 2) & 0x1;
	}

	assert(index < this->getLineCount());

	*pIndex = index;
	*pTArray = this->getData(index, offset);
	*pOffset = offset;
	*pLine = line;
}

void TVCache::lookupWrite(Addr addr, int type, int *pIndex, CacheLine** pLine, int* pOffset, Time** pTArray) {
	int index = this->getLineIndex(addr);
	int offset = ((UInt64)addr >> 2) & 0x1;
	assert(index < this->getLineCount());
	CacheLine* line = this->getTag(index);

	if (line->type == TimeTable::TYPE_64BIT && type == TimeTable::TYPE_32BIT) {
		// convert to 32bit	by duplicating 64bit info
		line->type = TimeTable::TYPE_32BIT;
		line->version[1] = line->version[0];
		line->lastSize[1] = line->lastSize[1];

		Time* option0 = this->getData(index, 0);
		Time* option1 = this->getData(index, 1);
		memcpy(option1, option0, sizeof(Time) * line->lastSize[0]);
	}


	//fprintf(stderr, "index = %d, tableSize = %d\n", tTableIndex, this->valueTable->row);
	*pIndex = index;
	*pTArray = this->getData(index, offset);
	*pLine = line;
	*pOffset = offset;
	return;
}

/*
 * TVCache Init/ Deinit
 */

void SkaduCache::init(int size_in_mb, bool compress, MShadowSkadu *mshadow) {
	if (size_in_mb == 0) {
		fprintf(stderr, "MShadowCacheInit: Bypass Cache\n"); 

	} else {
		tag_vector_cache.configure(size_in_mb, KConfigGetIndexSize());
	}
	this->use_compression = compress;
	this->mem_shadow = mshadow;
}

void SkaduCache::deinit() {
	if (KConfigUseSkaduCache() == FALSE) return;

	//printStat();
	MemPoolFreeSmall(tag_vector_cache.tagTable, sizeof(CacheLine) * tag_vector_cache.getLineCount());
	TableFree(tag_vector_cache.valueTable);
	//TableFree(valueTable[1]);
}

static int getStartInvalidLevel(Version lastVer, Version* vArray, Index size) {
	int firstInvalid = 0;
	if (size == 0)
		return 0;

	if (size > 2)
		MSG(TVCacheDebug, "\tgetStartInvalidLevel lastVer = %lld, newVer = %lld %lld \n", 
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
 * TVCache Evict / Flush / Resize 
 */

void SkaduCache::evict(int index, Version* vArray) {
	CacheLine* line = tag_vector_cache.getTag(index);
	Addr addr = line->tag;
	if (addr == 0x0)
		return;

	int lastSize = line->lastSize[0];
	int lastVer = line->version[0];
	int evictSize = getStartInvalidLevel(lastVer, vArray, lastSize);
	Time* tArray0 = tag_vector_cache.getData(index, 0);
	mem_shadow->evict(tArray0, line->tag, evictSize, vArray, line->type);

	if (line->type == TimeTable::TYPE_32BIT) {
		lastSize = line->lastSize[1];
		lastVer = line->version[1];
		evictSize = getStartInvalidLevel(lastVer, vArray, lastSize);
		Time* tArray1 = tag_vector_cache.getData(index, 1);
		mem_shadow->evict(tArray1, (char*)line->tag+4, evictSize, vArray, TimeTable::TYPE_32BIT);
	}
}

void SkaduCache::flush(Version* vArray) {
	int i;
	int size = tag_vector_cache.getLineCount();
	for (i=0; i<size; i++) {
		evict(i, vArray);
	}
		
}

void SkaduCache::resize(int newSize, Version* vArray) {
	flush(vArray);
	int size = tag_vector_cache.getSize();
	int oldDepth = tag_vector_cache.getDepth();
	int newDepth = oldDepth + 10;

	MSG(TVCacheDebug, "TVCacheResize from %d to %d\n", oldDepth, newDepth);
	fprintf(stderr, "TVCacheResize from %d to %d\n", oldDepth, newDepth);
	tag_vector_cache.configure(size, newDepth);
}

/*
 * Actual load / store handlers with TVCache
 */

void CacheLine::validateTag(Time* destAddr, Version* vArray, Index size) {
	int firstInvalid = getStartInvalidLevel(this->version[0], vArray, size);

	MSG(TVCacheDebug, "\t\tTVCacheValidateTag: invalid from level %d\n", firstInvalid);
	if (size > firstInvalid)
		bzero(&destAddr[firstInvalid], sizeof(Time) * (size - firstInvalid));
}

void SkaduCache::checkResize(int size, Version* vArray) {
	int oldDepth = tag_vector_cache.getDepth();
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
	CacheLine* entry = NULL;
	Time* destAddr = NULL;
	int offset = 0;
	int index = 0;
	tag_vector_cache.lookupRead(addr, type, &index, &entry, &offset, &destAddr);
	check(addr, destAddr, entry->lastSize[offset], 0);

	if (entry->isHit(addr)) {
		eventReadHit();
		MSG(TVCacheDebug, "\t cache hit at 0x%llx size = %d\n", destAddr, size);
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
	CacheLine* entry = NULL;
	Time* destAddr = NULL;
	int index = 0;
	int offset = 0;

	tag_vector_cache.lookupWrite(addr, type, &index, &entry, &offset, &destAddr);
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
		Time* duplicated = tag_vector_cache.getData(index, offset);
		memcpy(duplicated, tArray, sizeof(Time) * size);
	}
	entry->type = type;
	entry->tag = addr;
	entry->setVersion(offset, vArray[size-1]);
	entry->setValidSize(offset, size);

	check(addr, destAddr, size, 2);
}

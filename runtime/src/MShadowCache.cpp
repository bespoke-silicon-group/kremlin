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
#include "MShadowStat.h"
#include "Table.h"
#include "compression.h"
#include "config.h"

#include <string.h> // for memcpy

#define TVCacheDebug	0

/*
 * TVCache: cache for tag vectors
 */ 

typedef struct _CacheEntry {
	Addr tag;  
	Version version[2];
	int lastSize[2];	// required to know the region depth at eviction
	TimeTable::TableType type;

} CacheLine;

typedef struct _TVCache {
	CacheLine* tagTable;
	Table* valueTable;
} TVCache;

static TVCache tvCache;

typedef struct _CacheConfig {
	int  sizeMB;
	int  lineCount;
	int  lineShift;
	int  depth;
} CacheConfig;

static CacheConfig cacheConfig;

static int TVCacheGetSize() {
	return cacheConfig.sizeMB;
}

static int TVCacheGetLineCount() {
	return cacheConfig.lineCount;
}

static int TVCacheGetLineMask() {
	return cacheConfig.lineCount - 1;
}

static int TVCacheGetDepth() {
	return cacheConfig.depth;
}

static inline int getFirstOnePosition(int input) {
	int i;

	for (i=0; i<8 * sizeof(int); i++) {
		if (input & (1 << i))
			return i;
	}
	assert(0);
	return 0;
}

static int TVCacheGetLineShift() {
	return cacheConfig.lineShift;
}

static inline CacheLine* TVCacheGetTag(int index) {
	assert(index < TVCacheGetLineCount());
	return &(tvCache.tagTable[index]);
}


static inline Time* TVCacheGetData(int index, int offset) {
	return TableGetElementAddr(tvCache.valueTable, index*2 + offset, 0);
}


//static void TVCacheConfigure(int sizeMB) {
static void TVCacheConfigure(int sizeMB, int depth) {
	int lineSize = 8;
	int lineCount = sizeMB * 1024 * 1024 / lineSize;
	cacheConfig.sizeMB = sizeMB;
	cacheConfig.lineCount = lineCount;
	cacheConfig.lineShift = getFirstOnePosition(lineCount);
	cacheConfig.depth = depth;

	fprintf(stderr, "MShadowCacheInit: size: %d MB, lineNum %d, lineShift %d, depth %d\n", 
		sizeMB, lineCount, cacheConfig.lineShift, cacheConfig.depth);

	tvCache.tagTable = (CacheLine*)MemPoolCallocSmall(lineCount, sizeof(CacheLine)); // 64bit granularity
	tvCache.valueTable = TableCreate(lineCount * 2, cacheConfig.depth);  // 32bit granularity

	MSG(TVCacheDebug, "MShadowCacheInit: value Table created row %d col %d at addr 0x%x\n", 
		lineCount, KConfigGetIndexSize(), tvCache.valueTable[0].array);
}

static inline int getLineIndex(Addr addr) {
#if 0
	int nShift = 3; 	// 8 byte 
	int ret = (((UInt64)addr) >> nShift) & lineMask;
	assert(ret >= 0 && ret < lineNum);
#endif
	int nShift = 3;	
	int lineMask = TVCacheGetLineMask();
	int lineShift = TVCacheGetLineShift();
	int val0 = (((UInt64)addr) >> nShift) & lineMask;
	int val1 = (((UInt64)addr) >> (nShift + lineShift)) & lineMask;
	return val0 ^ val1;
}

static inline Version TVCacheGetVersion(CacheLine* line, int offset) {
	return line->version[offset];
}

static inline void TVCacheSetVersion(CacheLine* line, int offset, Version ver) {
	//int index = lineIndex >> CACHE_VERSION_SHIFT;
	//verTable[lineIndex] = ver;
	line->version[offset] = ver;
}

static inline void TVCacheSetValidSize(CacheLine* line, int offset, int size) {
	line->lastSize[offset] = size;
}

static inline Bool TVCacheIsHit(CacheLine* entry, Addr addr) {
	MSG(3, "isHit addr = 0x%llx, tag = 0x%llx, entry tag = 0x%llx\n",
		addr, entry->tag, entry->tag);

	return (((UInt64)entry->tag ^ (UInt64)addr) >> 3) == 0;
}

static inline void TVCachePrintLine(CacheLine* line) {
	fprintf(stderr, "addr 0x%llx, ver [%d, %d], lastSize [%d, %d], type %d\n",
		line->tag, line->version[0], line->version[1], line->lastSize[0], line->lastSize[1], line->type);
}


static void TVCacheLookupRead(Addr addr, int type, int* pIndex, CacheLine** pLine, int* pOffset, Time** pTArray) {
	int index = getLineIndex(addr);
	int offset = 0; 
	CacheLine* line = TVCacheGetTag(index);
	if (line->type == TimeTable::TYPE_32BIT && type == TimeTable::TYPE_64BIT) {
		// in this case, use the more recently one
		Time* option0 = TVCacheGetData(index, 0);
		Time* option1 = TVCacheGetData(index, 1);
		// check the first item only
		offset = (*option0 > *option1) ? 0 : 1;

	} else {
		offset = ((UInt64)addr >> 2) & 0x1;
	}

	assert(index < TVCacheGetLineCount());

	*pIndex = index;
	*pTArray = TVCacheGetData(index, offset);
	*pOffset = offset;
	*pLine = line;

	//TVCachePrintLine(*entry);
	//fprintf(stderr, "index = %d, tableSize = %d\n", tTableIndex, tvCache.valueTable->row);
	return;
}

static void TVCacheLookupWrite(Addr addr, int type, int *pIndex, CacheLine** pLine, int* pOffset, Time** pTArray) {
	int index = getLineIndex(addr);
	int offset = ((UInt64)addr >> 2) & 0x1;
	assert(index < TVCacheGetLineCount());
	CacheLine* line = TVCacheGetTag(index);

	if (line->type == TimeTable::TYPE_64BIT && type == TimeTable::TYPE_32BIT) {
		// convert to 32bit	by duplicating 64bit info
		line->type = TimeTable::TYPE_32BIT;
		line->version[1] = line->version[0];
		line->lastSize[1] = line->lastSize[1];

		Time* option0 = TVCacheGetData(index, 0);
		Time* option1 = TVCacheGetData(index, 1);
		memcpy(option1, option0, sizeof(Time) * line->lastSize[0]);
	}


	//fprintf(stderr, "index = %d, tableSize = %d\n", tTableIndex, tvCache.valueTable->row);
	*pIndex = index;
	*pTArray = TVCacheGetData(index, offset);
	*pLine = line;
	*pOffset = offset;
	return;
}

/*
 * TVCache Init/ Deinit
 */

void TVCacheInit(int cacheSizeMB) {
	if (cacheSizeMB == 0) {
		fprintf(stderr, "MShadowCacheInit: Bypass Cache\n"); 

	} else {
		TVCacheConfigure(cacheSizeMB, KConfigGetIndexSize());
	}
}

void TVCacheDeinit() {
	if (KConfigUseSkaduCache() == FALSE)
		return;

	//printStat();
	MemPoolFreeSmall(tvCache.tagTable, sizeof(CacheLine) * TVCacheGetLineCount());
	TableFree(tvCache.valueTable);
	//TableFree(valueTable[1]);
}

int getStartInvalidLevel(Version lastVer, Version* vArray, Index size) {
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

static void TVCacheEvict(int index, Version* vArray) {
	CacheLine* line = TVCacheGetTag(index);
	Addr addr = line->tag;
	if (addr == 0x0)
		return;

	int lastSize = line->lastSize[0];
	int lastVer = line->version[0];
	int evictSize = getStartInvalidLevel(lastVer, vArray, lastSize);
	Time* tArray0 = TVCacheGetData(index, 0);
	SkaduEvict(tArray0, line->tag, evictSize, vArray, line->type);

	if (line->type == TimeTable::TYPE_32BIT) {
		lastSize = line->lastSize[1];
		lastVer = line->version[1];
		evictSize = getStartInvalidLevel(lastVer, vArray, lastSize);
		Time* tArray1 = TVCacheGetData(index, 1);
		SkaduEvict(tArray1, (char*)line->tag+4, evictSize, vArray, TimeTable::TYPE_32BIT);
	}
}

static void TVCacheFlush(Version* vArray) {
	int i;
	int size = TVCacheGetLineCount();
	for (i=0; i<size; i++) {
		TVCacheEvict(i, vArray);
	}
		
}

static void TVCacheResize(int newSize, Version* vArray) {
	TVCacheFlush(vArray);
	int size = TVCacheGetSize();
	int oldDepth = TVCacheGetDepth();
	int newDepth = oldDepth + 10;

	MSG(TVCacheDebug, "TVCacheResize from %d to %d\n", oldDepth, newDepth);
	fprintf(stderr, "TVCacheResize from %d to %d\n", oldDepth, newDepth);
	TVCacheConfigure(size, newDepth);
}





/*
 * Actual load / store handlers with TVCache
 */

static inline void TVCacheValidateTag(CacheLine* line, Time* destAddr, Version* vArray, Index size) {
	int firstInvalid = getStartInvalidLevel(line->version[0], vArray, size);

	MSG(TVCacheDebug, "\t\tTVCacheValidateTag: invalid from level %d\n", firstInvalid);
	if (size > firstInvalid)
		bzero(&destAddr[firstInvalid], sizeof(Time) * (size - firstInvalid));
}

static void TVCacheCheckResize(int size, Version* vArray) {
	int oldDepth = TVCacheGetDepth();
	if (oldDepth < size) {
		TVCacheResize(oldDepth + 10, vArray);
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


Time* TVCacheGet(Addr addr, Index size, Version* vArray, TimeTable::TableType type) {
	TVCacheCheckResize(size, vArray);
	CacheLine* entry = NULL;
	Time* destAddr = NULL;
	int offset = 0;
	int index = 0;
	TVCacheLookupRead(addr, type, &index, &entry, &offset, &destAddr);
	check(addr, destAddr, entry->lastSize[offset], 0);

	if (TVCacheIsHit(entry, addr)) {
		eventReadHit();
		MSG(TVCacheDebug, "\t cache hit at 0x%llx size = %d\n", destAddr, size);
		TVCacheValidateTag(entry, destAddr, vArray, size);
		check(addr, destAddr, size, 1);

	} else {
		// Unfortunately, this access results in a miss
		// 1. evict a line	
		eventReadEvict();
		TVCacheEvict(index, vArray);
#if 0
		Version lastVer = entry->version[offset];

		int lastSize = entry->lastSize[offset];
		int testSize = (size < lastSize) ? size : lastSize;
		int evictSize = getStartInvalidLevel(lastVer, vArray, testSize);

		MSG(0, "\t CacheGet: evict size = %d, lastSize = %d, size = %d\n", 
			evictSize, entry->lastSize[offset], size);
#endif

		// 2. read line from MShadow to the evicted line
		SkaduFetch(addr, size, vArray, destAddr, type);
		entry->tag = addr;
		check(addr, destAddr, size, 2);
	}

	TVCacheSetVersion(entry, offset, vArray[size-1]);
	TVCacheSetValidSize(entry, offset, size);

	check(addr, destAddr, size, 3);
	return destAddr;
}

void TVCacheSet(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type) {
	TVCacheCheckResize(size, vArray);
	CacheLine* entry = NULL;
	Time* destAddr = NULL;
	int index = 0;
	int offset = 0;

	TVCacheLookupWrite(addr, type, &index, &entry, &offset, &destAddr);
#if 0
#ifndef NDEBUG
	if (hasVersionError(vArray, size)) {
		assert(0);
	}
#endif
#endif

	if (TVCacheIsHit(entry, addr)) {
		eventWriteHit();

	} else {
		eventWriteEvict();
		TVCacheEvict(index, vArray);
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
			SkaduEvict(destAddr, entry->tag, evictSize, vArray, entry->type);
#endif
	} 		

	// copy Timestamps
	memcpy(destAddr, tArray, sizeof(Time) * size);
	if (entry->type == TimeTable::TYPE_32BIT && type == TimeTable::TYPE_64BIT) {
		// corner case: duplicate the timestamp
		// not yet implemented
		Time* duplicated = TVCacheGetData(index, offset);
		memcpy(duplicated, tArray, sizeof(Time) * size);
	}
	entry->type = type;
	entry->tag = addr;
	TVCacheSetVersion(entry, offset, vArray[size-1]);
	TVCacheSetValidSize(entry, offset, size);

	check(addr, destAddr, size, 2);
}


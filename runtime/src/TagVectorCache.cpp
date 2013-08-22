#include <cassert>
#include "config.h"
#include "Table.h"
#include "TagVectorCache.h"
#include "TagVectorCacheLine.h"

static const int TV_CACHE_DEBUG_LVL = 0;

static inline int getFirstOnePosition(int input) {
	int i;

	for (i=0; i<8 * sizeof(int); i++) {
		if (input & (1 << i))
			return i;
	}
	assert(0);
	return 0;
}

TagVectorCacheLine* TagVectorCache::getTag(int index) {
	assert(index < getLineCount());
	return &tagTable[index];
}

Time* TagVectorCache::getData(int index, int offset) {
	return valueTable->getElementAddr(index*2 + offset, 0);
}

void TagVectorCache::configure(int new_size_in_mb, int new_depth) {
	// TODO: make sure we haven't configured before
	const int new_line_size = 8;
	int new_line_count = new_size_in_mb * 1024 * 1024 / new_line_size;
	this->size_in_mb = new_size_in_mb;
	this->line_count = new_line_count;
	this->line_shift = getFirstOnePosition(new_line_count);
	this->depth = new_depth;

	MSG(0, "TagVectorCache: size: %d MB, lineNum %d, lineShift %d, depth %d\n", 
		new_size_in_mb, new_line_count, this->line_shift, this->depth);

	tagTable = (TagVectorCacheLine*)MemPoolCallocSmall(new_line_count, sizeof(TagVectorCacheLine)); // 64bit granularity
	valueTable = new Table(new_line_count * 2, this->depth);  // 32bit granularity

	MSG(TV_CACHE_DEBUG_LVL, "MShadowCacheInit: value Table created row %d col %d\n", 
		new_line_count, kremlin_config.getNumProfiledLevels());
}

int TagVectorCache::getLineIndex(Addr addr) {
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



void TagVectorCache::lookupRead(Addr addr, int type, int* pIndex, TagVectorCacheLine** pLine, int* pOffset, Time** pTArray) {
	int index = this->getLineIndex(addr);
	int offset = 0; 
	TagVectorCacheLine* line = this->getTag(index);
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

void TagVectorCache::lookupWrite(Addr addr, int type, int *pIndex, TagVectorCacheLine** pLine, int* pOffset, Time** pTArray) {
	int index = this->getLineIndex(addr);
	int offset = ((UInt64)addr >> 2) & 0x1;
	assert(index < this->getLineCount());
	TagVectorCacheLine* line = this->getTag(index);

	if (line->type == TimeTable::TYPE_64BIT && type == TimeTable::TYPE_32BIT) {
		// convert to 32bit	by duplicating 64bit info
		line->type = TimeTable::TYPE_32BIT;
		line->version[1] = line->version[0];
		line->lastSize[1] = line->lastSize[1];

		Time* option0 = this->getData(index, 0);
		Time* option1 = this->getData(index, 1);
		memcpy(option1, option0, sizeof(Time) * line->lastSize[0]);
	}


	//fprintf(stderr, "index = %d, tableSize = %d\n", tTableIndex, this->valueTable->getRow());
	*pIndex = index;
	*pTArray = this->getData(index, offset);
	*pLine = line;
	*pOffset = offset;
	return;
}


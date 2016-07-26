#include <iostream>
#include <cassert>
#include "KremlinConfig.hpp"
#include "Table.hpp"
#include "TagVectorCache.hpp"
#include "TagVectorCacheLine.hpp"

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

TagVectorCache::TagVectorCache(int size, int dep) : size_in_mb(size), depth(dep)
{
	const int line_size = 8; // why 8? (-sat)
	int lc = size * 1024 * 1024 / line_size;
	this->line_count = lc;
	this->line_shift = getFirstOnePosition(lc);

	MSG(0, "TagVectorCache: size: %d MB, lineNum %d, lineShift %d, depth %d\n", 
		new_size_in_mb, lc, this->line_shift, this->depth);

	auto tt_del = [this](TagVectorCacheLine *p) {
		MemPoolFreeSmall(p, sizeof(TagVectorCacheLine) * this->getLineCount());
	};

	// 64-bit granularity
	tag_table = std::unique_ptr<TagVectorCacheLine[], std::function<void(TagVectorCacheLine*)>>
		((TagVectorCacheLine*)MemPoolCallocSmall(lc, sizeof(TagVectorCacheLine)), tt_del);

	value_table = std::unique_ptr<Table>(new Table(lc * 2, this->depth)); // 32bit granularity

	size_t lcs = sizeof(TagVectorCacheLine)*lc;

	MSG(TV_CACHE_DEBUG_LVL, "MShadowCacheInit: value Table created row %d col %d\n", 
		lc, kremlin_config.getNumProfiledLevels());
}

TagVectorCacheLine* TagVectorCache::getTag(int index) {
	assert(index < getLineCount());
	return &tag_table[index];
}

Time* TagVectorCache::getData(int index, int offset) {
	return value_table->getElementAddr(index*2 + offset, 0);
}


int TagVectorCache::getLineIndex(Addr addr) {
#if 0
	int nShift = 3; 	// 8 byte 
	int ret = (((uint64_t)addr) >> nShift) & lineMask;
	assert(ret >= 0 && ret < lineNum);
#endif
	int nShift = 3;	
	int lineMask = getLineMask();
	int lineShift = getLineShift();
	int val0 = (((uint64_t)addr) >> nShift) & lineMask;
	int val1 = (((uint64_t)addr) >> (nShift + lineShift)) & lineMask;
	return val0 ^ val1;
}



void TagVectorCache::lookupRead(Addr addr, TimeTable::TableType type, 
								int& pIndex, TagVectorCacheLine** pLine, 
								int& pOffset, Time** pTArray) {
	int index = this->getLineIndex(addr);
	int offset = 0; 
	TagVectorCacheLine* line = this->getTag(index);
	if (line->type == TimeTable::TableType::TYPE_32BIT && type == TimeTable::TableType::TYPE_64BIT) {
		// in this case, use the more recently one
		Time* option0 = this->getData(index, 0);
		Time* option1 = this->getData(index, 1);
		// check the first item only
		offset = (*option0 > *option1) ? 0 : 1;

	} else {
		offset = ((uint64_t)addr >> 2) & 0x1;
	}

	assert(index < this->getLineCount());

	pIndex = index;
	*pTArray = this->getData(index, offset);
	pOffset = offset;
	*pLine = line;
}

void TagVectorCache::lookupWrite(Addr addr, TimeTable::TableType type, 
									int& pIndex, TagVectorCacheLine** pLine, 
									int& pOffset, Time** pTArray) {
	int index = this->getLineIndex(addr);
	int offset = ((uint64_t)addr >> 2) & 0x1;
	assert(index < this->getLineCount());
	TagVectorCacheLine* line = this->getTag(index);

	if (line->type == TimeTable::TableType::TYPE_64BIT && type == TimeTable::TableType::TYPE_32BIT) {
		// convert to 32bit	by duplicating 64bit info
		line->type = TimeTable::TableType::TYPE_32BIT;
		line->version[1] = line->version[0];
		line->lastSize[1] = line->lastSize[1];

		Time* option0 = this->getData(index, 0);
		Time* option1 = this->getData(index, 1);
		memcpy(option1, option0, sizeof(Time) * line->lastSize[0]);
	}


	//fprintf(stderr, "index = %d, tableSize = %d\n", tTableIndex, this->value_table->getRow());
	pIndex = index;
	*pTArray = this->getData(index, offset);
	*pLine = line;
	pOffset = offset;
	return;
}


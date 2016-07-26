#ifndef TAG_VECTOR_CACHE_H
#define TAG_VECTOR_CACHE_H

#include <memory>
#include <functional>
#include "ktypes.h"
#include "TimeTable.hpp" // for TimeTable::TableType... (TODO: avoid this include)

class TagVectorCacheLine;
class Table;

/*! \brief Cache for tag vectors */ 
class TagVectorCache {
private:
	int  size_in_mb;
	int  line_count;
	int  line_shift;
	int  depth;
	std::unique_ptr<TagVectorCacheLine[], 
		std::function<void(TagVectorCacheLine*)>> tag_table;
	std::unique_ptr<Table> value_table;

public:
	TagVectorCache(int size, int dep);

	int getSize() { return size_in_mb; }
	int getLineCount() { return line_count; }
	int getLineMask() { return line_count - 1; }
	int getDepth() { return depth; }
	int getLineShift() { return line_shift; }

	TagVectorCacheLine* getTag(int index);
	Time* getData(int index, int offset);
	int getLineIndex(Addr addr);

	void lookupRead(Addr addr, TimeTable::TableType type, 
					int& pIndex, TagVectorCacheLine** pLine, 
					int& pOffset, Time** pTArray);
	void lookupWrite(Addr addr, TimeTable::TableType type, 
					int& pIndex, TagVectorCacheLine** pLine, 
					int& pOffset, Time** pTArray);
};

#endif

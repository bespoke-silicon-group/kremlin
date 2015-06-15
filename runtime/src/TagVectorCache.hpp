#ifndef TAG_VECTOR_CACHE_H
#define TAG_VECTOR_CACHE_H

#include "ktypes.h"

class TagVectorCacheLine;
class Table;

/*! \brief Cache for tag vectors */ 
class TagVectorCache {
private:
	int  size_in_mb;
	int  line_count;
	int  line_shift;
	int  depth;

public:
	TagVectorCacheLine* tagTable;
	Table* valueTable;

	int getSize() { return size_in_mb; }
	int getLineCount() { return line_count; }
	int getLineMask() { return line_count - 1; }
	int getDepth() { return depth; }
	int getLineShift() { return line_shift; }

	TagVectorCacheLine* getTag(int index);
	Time* getData(int index, int offset);
	int getLineIndex(Addr addr);

	void configure(int size_in_mb, int depth);
	void lookupRead(Addr addr, int type, int* pIndex, TagVectorCacheLine** pLine, int* pOffset, Time** pTArray);
	void lookupWrite(Addr addr, int type, int *pIndex, TagVectorCacheLine** pLine, int* pOffset, Time** pTArray);
};

#endif

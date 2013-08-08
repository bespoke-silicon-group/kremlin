#ifndef TAG_VECTOR_CACHE_LINE_H
#define TAG_VECTOR_CACHE_LINE_H

#include "MShadowSkadu.h" // needed for TimeTable::TableType :(

/*! \brief Single line in a tag vector cache */ 
class TagVectorCacheLine {
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

	void print() {
		fprintf(stderr, "addr 0x%p, ver [%llu, %llu], lastSize [%d, %d], type %d\n",
			this->tag, this->version[0], this->version[1], this->lastSize[0], this->lastSize[1], this->type);
	}

	bool isHit(Addr addr);
	void validateTag(Time* destAddr, Version* vArray, Index size);
};

#endif

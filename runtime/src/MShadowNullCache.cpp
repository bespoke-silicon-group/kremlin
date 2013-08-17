#include "LevelTable.hpp"
#include "MShadowSkadu.h"
#include "MShadowNullCache.h"
#include "compression.h"

static Time tempArray[1000];

Time* NullCache::get(Addr addr, Index size, Version* vArray, TimeTable::TableType type) {
	LevelTable* lTable = mem_shadow->getLevelTable(addr, vArray);	
	Index i;
	for (i=0; i<size; i++) {
		tempArray[i] = lTable->getTimeForAddrAtLevel(i, addr, vArray[i]);
	}

	if (use_compression) mem_shadow->getCompressionBuffer()->touch(lTable);

	return tempArray;	
}

void NullCache::set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type) {
	LevelTable* lTable = mem_shadow->getLevelTable(addr, vArray);	
	assert(lTable != NULL);
	Index i;
	for (i=0; i<size; i++) {
		lTable->setTimeForAddrAtLevel(i, addr, vArray[i], tArray[i], type);
	}

	if (use_compression) mem_shadow->getCompressionBuffer()->touch(lTable);
}

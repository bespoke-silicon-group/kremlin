#include <cassert>
#include "debug.h"
#include "ktypes.h"
#include "MShadowStat.h" // for mshadow event counters
#include "LevelTable.hpp"
#include "compression.h"

void* LevelTable::operator new(size_t size) {
	return MemPoolAllocSmall(sizeof(LevelTable));
}

void LevelTable::operator delete(void* ptr) {
	MemPoolFreeSmall(ptr, sizeof(LevelTable));
}

LevelTable::LevelTable() : code(0xDEADBEEF) {
	memset(this->versions, 0, LevelTable::MAX_LEVEL * sizeof(Version));
	memset(this->time_tables, 0, LevelTable::MAX_LEVEL * sizeof(TimeTable*));
}

LevelTable::~LevelTable() {
	for (unsigned i = 0; i < LevelTable::MAX_LEVEL; ++i) {
		TimeTable *t = time_tables[i];
		if (t != NULL) {
			delete t;
			t = NULL;
		}
	}
}

Time LevelTable::getTimeForAddrAtLevel(Index level, Addr addr, Version curr_ver) {
	assert(level < LevelTable::MAX_LEVEL);

	TimeTable *table = this->getTimeTableAtLevel(level);
	Version stored_ver = this->versions[level];

	Time ret = 0;
	if (table != NULL && stored_ver == curr_ver) {
		ret = table->getTimeAtAddr(addr);
		MSG(0, "\t\tlv %d: \tversion = [%d, %d] value = %d\n", 
			level, stored_ver, curr_ver, ret);
	} 

	return ret;
}

void LevelTable::setTimeForAddrAtLevel(Index level, Addr addr, 
										Version curr_ver, Time value, 
										TimeTable::TableType type) {
	assert(level < LevelTable::MAX_LEVEL);

	TimeTable *table = this->getTimeTableAtLevel(level);
	Version stored_ver = this->getVersionAtLevel(level);
	eventLevelWrite(level);

	// no timeTable exists so create it
	if (table == NULL) {
		table = new TimeTable(type);
		table->setTimeAtAddr(addr, value, type);
		this->setTimeTableAtLevel(level, table); 
		eventTimeTableNewAlloc(level, type);
	} else {
		// convert the table if needed
		// XXX: what about if table is 32-bit but type is 64-bit?
		if (type == TimeTable::TYPE_32BIT && table->type == TimeTable::TYPE_64BIT) {
			TimeTable *old = table;
			table = table->create32BitClone();
			eventTimeTableConvertTo32();
			delete old;
			old = NULL;
		}

		if (stored_ver == curr_ver) {
			assert(table != NULL);
			table->setTimeAtAddr(addr, value, type);
		} else {
			// exists but version is old so clean it and reuse
			table->clean();
			table->setTimeAtAddr(addr, value, type);
		}
	}
	this->setVersionAtLevel(level, curr_ver);
}


unsigned LevelTable::findLowestInvalidIndex(Version *curr_versions) {
	assert(curr_versions != NULL);

	unsigned lowest_valid = 0;
	while(lowest_valid < LevelTable::MAX_LEVEL 
		&& this->time_tables[lowest_valid] != NULL 
		&& this->versions[lowest_valid] >= curr_versions[lowest_valid]) {
		++lowest_valid;
	}

	assert(lowest_valid < LevelTable::MAX_LEVEL); // TODO: should be exception
	return lowest_valid;
}

void LevelTable::cleanTimeTablesFromLevel(Index start_level) {
	for(unsigned i = start_level; i < LevelTable::MAX_LEVEL; ++i) {
		TimeTable *t = this->time_tables[i];
		if (t != NULL) {
			delete t;
			t = NULL;
			this->time_tables[i] = NULL;
		}
	}
}

void LevelTable::collectGarbageWithinBounds(Version *curr_versions, 
											unsigned end_index) {
	assert(curr_versions != NULL);
	assert(end_index < LevelTable::MAX_LEVEL);

	for (unsigned i = 0; i < end_index; ++i) {
		TimeTable *table = this->time_tables[i];
		if (table == NULL)
			continue;

		Version ver = this->versions[i];
		if (ver < curr_versions[i]) {
			// out of date
			delete table;
			table = NULL;
			this->time_tables[i] = NULL;
		}
	}

	this->cleanTimeTablesFromLevel(end_index);
}

void LevelTable::collectGarbageUnbounded(Version *curr_versions) {
	assert(curr_versions != NULL);

	int lii = this->findLowestInvalidIndex(curr_versions);
	this->cleanTimeTablesFromLevel(lii);
}

UInt64 LevelTable::compress() {
	assert(this->code == 0xDEADBEEF);
	assert(!isCompressed());

	MSG(4,"compressing LevelTable (addr: 0x%p)\n", this);

	TimeTable* tt1 = this->time_tables[0];

	if (tt1 == NULL) {
		this->compressed = true;
		return 0;
	}

	UInt64 compressionSavings = 0;
	lzo_uint srcLen = sizeof(Time)*TimeTable::TIMETABLE_SIZE/2; // XXX assumes 8 bytes
	lzo_uint compLen = 0;

	Time* diffBuffer = (Time*)MemPoolAlloc();
	void* compressedData;

	for(unsigned i = LevelTable::MAX_LEVEL-1; i >=1; --i) {
		// step 1: create/fill in time difference table
		TimeTable* tt2 = this->time_tables[i];
		TimeTable* ttPrev = this->time_tables[i-1];
		if(tt2 == NULL)
			continue;

		assert(tt2 != NULL);
		assert(ttPrev != NULL);

		int j;
		for(j = 0; j < TimeTable::TIMETABLE_SIZE/2; ++j) {
			diffBuffer[j] = ttPrev->array[j] - tt2->array[j];
		}

		// step 2: compress diffs
		makeDiff(diffBuffer);
		compressedData = compressData((UInt8*)diffBuffer, srcLen, &compLen);
		compressionSavings += (srcLen - compLen);
		tt2->size = compLen;

		// step 3: profit
		MemPoolFree(tt2->array); // XXX: comment this out if using tArrayBackup
		tt2->array = (Time*)compressedData;
	}
	Time* level0Array = (Time*)MemPoolAlloc();
	memcpy(level0Array, tt1->array, srcLen);
	makeDiff(tt1->array);
	compressedData = compressData((UInt8*)tt1->array, srcLen, &compLen);
	MemPoolFree(tt1->array);
	//Time* level0Array = tt1->array;
	tt1->array = (Time*)compressedData;
	tt1->size = compLen;
	compressionSavings += (srcLen - compLen);


	MemPoolFree(level0Array);  // XXX: comment this out if using tArrayBackup
	MemPoolFree(diffBuffer);

	this->compressed = true;

	assert(isCompressed());
	assert(this->code == 0xDEADBEEF);
	return compressionSavings;
}

UInt64 LevelTable::decompress() {
	assert(this->code == 0xDEADBEEF);
	assert(isCompressed());

	//fprintf(stderr,"[LevelTable] decompressing LevelTable (%p)\n",this);
	UInt64 decompressionCost = 0;
	lzo_uint srcLen = sizeof(Time)*TimeTable::TIMETABLE_SIZE/2;
	lzo_uint uncompLen = srcLen;

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = this->time_tables[0];
	if (tt1 == NULL) {
		this->compressed = false;
		return 0;
	}
	int compressedSize = tt1->size;

	Time* decompedArray = (Time*)MemPoolAlloc();
	decompressData((UInt8*)decompedArray, (UInt8*)tt1->array, compressedSize, &uncompLen);
	restoreDiff((Time*)decompedArray);

	tt1->array = decompedArray;
	decompressionCost += (srcLen - compressedSize);
	tt1->size = srcLen;

	//tArrayIsDiff(tt1->array, this->tArrayBackup[0]);

	Time *diffBuffer = (Time*)MemPoolAlloc();

	for(unsigned i = 1; i < LevelTable::MAX_LEVEL; ++i) {
		TimeTable* tt2 = this->time_tables[i];
		TimeTable* ttPrev = this->time_tables[i-1];
		if(tt2 == NULL) 
			break;

		assert(tt2 != NULL);
		assert(ttPrev != NULL);

		// step 1: decompress time different table, 
		// the src buffer will be freed in decompressData
		uncompLen = srcLen;
		decompressData((UInt8*)diffBuffer, (UInt8*)tt2->array, tt2->size, &uncompLen);
		restoreDiff((Time*)diffBuffer);
		assert(srcLen == uncompLen);
		decompressionCost += (srcLen - tt2->size);

		// step 2: add diffs to base TimeTable
		tt2->array = (Time*)MemPoolAlloc();
		tt2->size = srcLen;

		for(unsigned j = 0; j < TimeTable::TIMETABLE_SIZE/2; ++j) {
			assert(diffBuffer[j] >= 0);
			tt2->array[j] = ttPrev->array[j] - diffBuffer[j];
		}
	#if 0
		if (memcmp(tt2->array, this->tArrayBackup[i], uncompLen) != 0) {
			fprintf(stderr, "error at level %d\n", i);
			assert(0);
		}
	#endif
//		assert(memcmp(tt2->array, this->tArrayBackup[i], uncompLen) == 0);
		//tArrayIsDiff(tt2->array, this->tArrayBackup[i]);
	}

	MemPoolFree(diffBuffer);
	this->compressed = false;

	assert(this->code == 0xDEADBEEF);
	assert(!isCompressed());
	return decompressionCost;
}

void LevelTable::makeDiff(Time *array) {
	assert(array != NULL);
	unsigned size = TimeTable::TIMETABLE_SIZE / 2;

	for (unsigned i = size-1; i >= 1; --i) {
		array[i] = array[i] - array[i-1];
	}
}

void LevelTable::restoreDiff(Time *array) {
	assert(array != NULL);
	unsigned size = TimeTable::TIMETABLE_SIZE / 2;
	for (unsigned i = 1; i < size; ++i) {
		array[i] += array[i-1];
	}
}

unsigned LevelTable::getDepth() {
	// TODO: assert for NULL TimeTable* precondition
	for (unsigned i = 0; i < LevelTable::MAX_LEVEL; ++i) {
		TimeTable* t = this->time_tables[i];
		if (t == NULL)
			return i;
	}
	assert(0);
	return -1;
}

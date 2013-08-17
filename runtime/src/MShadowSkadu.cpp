#include <cassert>
#include <stdio.h>

#include "kremlin.h"
#include "config.h"
#include "debug.h"
#include "MemMapAllocator.h"

#include "Table.h"
#include "LevelTable.hpp"
#include "MemorySegment.hpp"
#include "MShadowSkadu.h"
#include "MShadowStat.h"
#include "compression.h"

#include "MShadowCache.h"
#include "MShadowNullCache.h"

#include <string.h> // for memset

#include <vector>

/*!
 * @brief A sparse table that tracks 4GB memory chunks being used.
 *
 * Since 64bit address is very sparsely used in a program,
 * we use a sparse table to reduce the memory requirement of the table.
 * Although the walk-through of a table might be pricey,
 * the use of cache will make the frequency of walk-through very low.
 */
class SparseTableElement {
public:
	UInt32 	addrHigh;	// upper 32bit in 64bit addr
	MemorySegment* segTable;
};

/*!
 * @brief A class to efficiently support 64-bit address space
 */
class SparseTable {
public:
	static const unsigned int NUM_ENTRIES = 32;

	std::vector<SparseTableElement> entry;
	int writePtr;

	void init() { 
		entry.resize(SparseTable::NUM_ENTRIES);
		writePtr = 0;
	}

	void deinit() {
		for (int i = 0; i < writePtr; i++) {
			SparseTableElement* e = &entry[i];
			delete e->segTable;		
			e->segTable = NULL;
			eventSegTableFree();
		}
	}

	SparseTableElement* getElement(Addr addr) {
		UInt32 highAddr = (UInt32)((UInt64)addr >> 32);

		// walk-through SparseTable
		for (int i=0; i < writePtr; i++) {
			if (entry[i].addrHigh == highAddr) {
				//MSG(0, "SparseTable Found an existing entry..\n");
				return &entry[i];	
			}
		}

		// not found - create an entry
		MSG(0, "SparseTable Creating a new Entry..\n");

		SparseTableElement* ret = &entry[writePtr];
		ret->addrHigh = highAddr;
		ret->segTable = new MemorySegment();
		eventSegTableAlloc();
		writePtr++;
		return ret;
	}
	
};

/*
 * Compression Configuration
 * instead of calling KConfigGetCompression repeatedly, 
 * store the result and reuse.
 */
static int _useCompression = 0;
static inline int useCompression() {
	return _useCompression;
}

static void setCompression() {
	_useCompression = KConfigGetCompression();
}


void* TimeTable::operator new(size_t size) {
	return MemPoolAllocSmall(sizeof(TimeTable));
}

void TimeTable::operator delete(void* ptr) {
	MemPoolFreeSmall(ptr, sizeof(TimeTable));
}


unsigned TimeTable::getIndex(Addr addr) {
	const int WORD_SHIFT = 2;
	int ret = ((UInt64)addr >> WORD_SHIFT) & TimeTable::TIMETABLE_MASK;
	if (this->type == TYPE_64BIT) ret >>= 1;

	assert((this->type == TYPE_64BIT && ret < TimeTable::TIMETABLE_SIZE/2) 
			|| ret < TimeTable::TIMETABLE_SIZE);
	return ret;
}

TimeTable::TimeTable(TimeTable::TableType size_type) : type(size_type) {
	this->array = (Time*)MemPoolAlloc();
	unsigned size = TimeTable::GetNumEntries(size_type);
	memset(this->array, 0, sizeof(Time) * size);

	this->size = sizeof(Time) * TIMETABLE_SIZE / 2; // XXX: hardwired for 64?

	eventTimeTableAlloc(size_type, this->size);
	assert(this->array != NULL);
}


// XXX: this used to have an "isCompressed" arg that wasn't used
TimeTable::~TimeTable() {
	eventTimeTableFree(this->type, this->size);

	MemPoolFree(this->array);
	this->array = NULL;
}

// TODO: Replace with a function that modifies this TimeTable rather than
// creating a new one
TimeTable* TimeTable::create32BitClone() {
	assert(this->type == TimeTable::TYPE_64BIT);
	TimeTable* ret = new TimeTable(TimeTable::TYPE_32BIT);
	for (unsigned i = 0; i < TIMETABLE_SIZE/2; ++i) {
		ret->array[i*2] = this->array[i];
		ret->array[i*2 + 1] = this->array[i];
	}
	return ret;
}

void TimeTable::setTimeAtAddr(Addr addr, 
								Time time, 
								TimeTable::TableType access_type) {
	assert(addr != NULL);

	unsigned index = this->getIndex(addr);

	MSG(3, "TimeTableSet to addr 0x%llx with index %d\n", 
			&array[index], index);
	MSG(3, "\t table addr = 0x%llx, array addr = 0x%llx\n", 
			this, &array[0]);

	array[index] = time;
	if (this->type == TimeTable::TYPE_32BIT 
		&& access_type == TimeTable::TYPE_64BIT) {
		array[index+1] = time;
	}
}

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


/*
 * Garbage collection related logic
 */
void MShadowSkadu::initGarbageCollector(unsigned period) {
	MSG(3, "set garbage collection period to %u\n", period);
	next_gc_time = period;
	garbage_collection_period = period;
	if (period == 0) next_gc_time = 0xFFFFFFFFFFFFFFFF;
}

void MShadowSkadu::runGarbageCollector(Version* curr_versions, int size) {
	eventGC();
	for (unsigned i = 0; i < SparseTable::NUM_ENTRIES; ++i) {
		MemorySegment* table = sparse_table->entry[i].segTable;	
		if (table == NULL)
			continue;
		
		for (unsigned j = 0; j < MemorySegment::getNumLevelTables(); ++j) {
			LevelTable* lTable = table->getLevelTableAtIndex(j);
			if (lTable != NULL) {
				lTable->collectGarbageWithinBounds(curr_versions, size);
			}
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

	MSG(4,"[LevelTable] compressing LevelTable (%p)\n",l_table);

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


/*
 * LevelTable operations
 */

LevelTable* MShadowSkadu::getLevelTable(Addr addr, Version* vArray) {
	SparseTableElement* sEntry = sparse_table->getElement(addr);
	MemorySegment* segTable = sEntry->segTable;
	assert(segTable != NULL);
	unsigned segIndex = MemorySegment::GetIndex(addr);
	LevelTable* lTable = segTable->getLevelTableAtIndex(segIndex);
	if (lTable == NULL) {
		lTable = new LevelTable();
		if (useCompression()) {
			int compressGain = compression_buffer->add(lTable);
			eventCompression(compressGain);
		}
		segTable->setLevelTableAtIndex(lTable, segIndex);
		eventLevelTableAlloc();
	}
	
	if(useCompression() && lTable->isCompressed()) {
		lTable->collectGarbageUnbounded(vArray);
		int gain = compression_buffer->decompress(lTable);
		eventCompression(gain);
	}


	return lTable;
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

static inline int hasVersionError(Version* vArray, int size) {
#ifndef NDEBUG
	int i;
	for (i=1; i<size; i++) {
		if (vArray[i-1] > vArray[i])
			return 1;
	}
#endif
	return 0;
}

void* MemorySegment::operator new(size_t size) {
	return MemPoolAllocSmall(sizeof(MemorySegment));
}
void MemorySegment::operator delete(void* ptr) {
	MemPoolFreeSmall(ptr, sizeof(MemorySegment));
}

MemorySegment::MemorySegment() {
	memset(this->level_tables, 0, 
			MemorySegment::NUM_ENTRIES * sizeof(LevelTable*));
}

MemorySegment::~MemorySegment() {
	for (unsigned i = 0; i < MemorySegment::NUM_ENTRIES; ++i) {
		LevelTable *l = level_tables[i];
		if (l != NULL) {
			delete l;
			l = NULL;
		}
	}
}

/*
 * Fetch / Evict from TVCache to TVStorage
 */

void MShadowSkadu::evict(Time* tArray, Addr addr, int size, Version* vArray, TimeTable::TableType type) {
	if (addr == NULL)
		return;


	MSG(0, "\tTVCacheEvict 0x%llx, size=%d, effectiveSize=%d \n", addr, size, size);
		
	LevelTable* lTable = this->getLevelTable(addr,vArray);
	for (int i=0; i<size; i++) {
		eventEvict(i);
		if (tArray[i] == 0ULL) {
			break;
		}
		lTable->setTimeForAddrAtLevel(i, addr, vArray[i], tArray[i], type);
		MSG(0, "\t\toffset=%d, version=%d, value=%d\n", i, vArray[i], tArray[i]);
	}
	eventCacheEvict(size, size);

	//fprintf(stderr, "\tTVCacheEvict lTable=%llx, 0x%llx, size=%d, effectiveSize=%d \n", lTable, addr, size, startInvalid);
	if (useCompression())
		compression_buffer->touch(lTable);
	
	
	check(addr, tArray, size, 3);
}

void MShadowSkadu::fetch(Addr addr, Index size, Version* vArray, Time* destAddr, TimeTable::TableType type) {
	MSG(0, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	//fprintf(stderr, "\tTVCacheFetch 0x%llx, size %d \n", addr, size);
	LevelTable* lTable = this->getLevelTable(addr, vArray);

	int i;
	for (i=0; i<size; i++) {
		destAddr[i] = lTable->getTimeForAddrAtLevel(i, addr, vArray[i]);
	}

	if (useCompression())
		compression_buffer->touch(lTable);
}


/*
 * Entry point functions from Kremlin
 */

Time* MShadowSkadu::get(Addr addr, Index size, Version* vArray, UInt32 width) {
	if (size < 1) return NULL;

	//TimeTable::TableType type = (width > 4) ? TimeTable::TYPE_64BIT: TimeTable::TYPE_32BIT;
	TimeTable::TableType type = TimeTable::TYPE_64BIT; 

	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "MShadowGet 0x%llx, size %d \n", tAddr, size);
	eventRead();
	return cache->get(tAddr, size, vArray, type);
}

void MShadowSkadu::set(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
	MSG(0, "MShadowSet 0x%llx, size %d [", addr, size);
	if (size < 1)
		return;

	if (getActiveTimeTableSize() >= next_gc_time) {
		runGarbageCollector(vArray, size);
		//next_gc_time = stat.nTimeTableActive + garbage_collection_period;
		next_gc_time += garbage_collection_period;
	}

	//TimeTable::TableType type = (width > 4) ? TimeTable::TYPE_64BIT: TimeTable::TYPE_32BIT;
	TimeTable::TableType type = TimeTable::TYPE_64BIT;


	Addr tAddr = (Addr)((UInt64)addr & ~(UInt64)0x7);
	MSG(0, "]\n");
	eventWrite();
	cache->set(tAddr, size, vArray, tArray, type);
}

/*
 * Init / Deinit
 */

void MShadowSkadu::init() {
	
	int cacheSizeMB = KConfigGetSkaduCacheSize();
	fprintf(stderr,"[kremlin] MShadow Init with cache %d MB, TimeTableSize = %ld\n",
		cacheSizeMB, sizeof(TimeTable));

	if (KConfigUseSkaduCache() == TRUE) 
		cache = new SkaduCache();
	else
		cache = new NullCache();

	cache->init(cacheSizeMB, KConfigGetCompression(), this);

	unsigned size = TimeTable::GetNumEntries(TimeTable::TYPE_64BIT);
	MemPoolInit(1024, size * sizeof(Time));
	
	initGarbageCollector(KConfigGetGCPeriod());
 
	sparse_table = new SparseTable();
	sparse_table->init();

	compression_buffer = new CBuffer();
	compression_buffer->init(KConfigGetCBufferSize());
	setCompression();
}


void MShadowSkadu::deinit() {
	cache->deinit();
	delete cache;
	cache = NULL;
	compression_buffer->deinit();
	delete compression_buffer;
	compression_buffer = NULL;
	MShadowStatPrint();
	sparse_table->deinit();
}

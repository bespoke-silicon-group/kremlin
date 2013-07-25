#include <map>

#include "kremlin.h"
#include "compression.h"
#include "MShadowSkadu.h"
#include "uthash.h"
#include "config.h"
#include "minilzo.h"


static UInt64 _compSrcSize;
static UInt64 _compDestSize;
static UInt64 totalAccess;
static UInt64 totalEvict;

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);


/*! \brief Compress data using LZO library
 *
 * \pre Input data is valid and non-zero sized.
 * \param src The data to be compressed
 * \param src_size The size of input data (in bytes)
 * \param[out] dest_size The size data after compressed
 * \return Pointer to the beginning of compressed data
 */
static UInt8* compressData(UInt8* src, lzo_uint src_size, lzo_uintp dest_size) {
	assert(src != NULL);
	assert(src_size > 0);
	assert(dest_size != NULL);

	//XXX need a specialized memory allocator, for now, just check the 

	//UInt8* dest = MemPoolAlloc();
	UInt8* dest = (UInt8*)malloc(src_size);
	int result = lzo1x_1_compress(src, src_size, dest, dest_size, wrkmem);
	assert(result == LZO_E_OK);
	//memcpy(dest, src, src_size);
	//*dest_size  = src_size;

	//fprintf(stderr, "compressed from %d to %d\n", src_size, *dest_size);
	_compSrcSize += src_size;
	_compDestSize += *dest_size;
	return dest;
}

/*! \brief Decompress data using LZO library
 *
 * \param dest Chunk of memory where decompressed data is written
 * \param src The data to be decompressed
 * \param src_size Size of the compressed data (in bytes)
 * \param[out] dest_size Size of the decompressed data (in bytes)
 */
static void decompressData(UInt8* dest, UInt8* src, lzo_uint src_size, lzo_uintp dest_size) {
	assert(src != NULL);
	assert(dest != NULL);
	assert(src_size > 0);
	assert(dest_size != NULL);

	int result = lzo1x_decompress(src, src_size, dest, dest_size, NULL);
	assert(result == LZO_E_OK);
	//memcpy(dest, src, src_size);
	//*dest_size = src_size;

	//fprintf(stderr, "decompressed from %d to %d\n", src_size, *dest_size);
	//MemPoolFree(src);
	free(src);
}

/*! \brief Modify array so elements are difference between that element and
 * the previous element.
 *
 * \param[in,out] array The array to convert
 */
void makeDiff(Time* array) {
	int size = TimeTable::TIMETABLE_SIZE / 2;

	for (int i=size-1; i>=1; --i) {
		array[i] = array[i] - array[i-1];
	}
}

/*! \brief Perform inverse operation of makeDiff
 *
 * \param[in,out] array The array to convert
 */
void restoreDiff(Time* array) {
	int size = TimeTable::TIMETABLE_SIZE / 2;
	int i;

	for (i=1; i<size; i++) {
		array[i] += array[i-1];
	}
}

/*! \brief Unknown.
 *
 * \param a The column???
 * \param b The row???
 * \return Offset in table given row and column.
 */
static inline int getByteOffset(int a, int b) {
	int size = TimeTable::TIMETABLE_SIZE / 2;
	return b * size + a;
}


// TODO: make this a member function of Level Table
/*! \brief Get the number of entries in Level table.
 *
 * \param l_table The table to get number of entries from.
 * \return Number of entries in specified level table.
 */
int getTimeTableSize(LevelTable* l_table) {
	for (unsigned i = 0; i < LevelTable::MAX_LEVEL; ++i) {
		TimeTable* table = l_table->tArray[i];
		if (table == NULL)
			return i;
	}
	assert(0);
	return -1;
}

/*! \brief Compress a level table.
 *
 * \param l_table The level table to be compressed.
 * \return The number of bytes saved by compression.
 * \remark It is assumed you already garbage collected the table, otherwise
 * you are going to be compressing out of data data.
 */
static UInt64 compressLevelTable(LevelTable* l_table) {
	//fprintf(stderr,"[LevelTable] compressing LevelTable (%p)\n",l_table);
	if (l_table->code != 0xDEADBEEF) {
		fprintf(stderr, "LevelTable addr = 0x%llx\n", l_table);
		assert(0);
	}
	assert(l_table->code == 0xDEADBEEF);
	assert(l_table->isCompressed == 0);

	TimeTable* tt1 = l_table->tArray[0];

	if (tt1 == NULL) {
		l_table->isCompressed = 1;
		return 0;
	}


	UInt64 compressionSavings = 0;
	lzo_uint srcLen = sizeof(Time)*TimeTable::TIMETABLE_SIZE/2; // XXX assumes 8 bytes
	lzo_uint compLen = 0;

	Time* diffBuffer = (Time*)MemPoolAlloc();
	void* compressedData;

	for(unsigned i = LevelTable::MAX_LEVEL-1; i >=1; --i) {
		// step 1: create/fill in time difference table
		TimeTable* tt2 = l_table->tArray[i];
		TimeTable* ttPrev = l_table->tArray[i-1];
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

	l_table->isCompressed = 1;
	return compressionSavings;
}


/*! \brief Decompress a level table.
 *
 * \param l_table The level table to be decompressed.
 * \return The number of bytes lost by decompression.
 */
static UInt64 decompressLevelTable(LevelTable* l_table) {

	if (l_table->code != 0xDEADBEEF) {
		fprintf(stderr, "LevelTable addr = 0x%llx\n", l_table);
		assert(0);
	}
	assert(l_table->code == 0xDEADBEEF);
	assert(l_table->isCompressed == 1);

	//fprintf(stderr,"[LevelTable] decompressing LevelTable (%p)\n",l_table);
	UInt64 decompressionCost = 0;
	lzo_uint srcLen = sizeof(Time)*TimeTable::TIMETABLE_SIZE/2;
	lzo_uint uncompLen = srcLen;

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = l_table->tArray[0];
	if (tt1 == NULL) {
		l_table->isCompressed = 0;
		return 0;
	}
	int compressedSize = tt1->size;

	Time* decompedArray = (Time*)MemPoolAlloc();
	decompressData((UInt8*)decompedArray, (UInt8*)tt1->array, compressedSize, &uncompLen);
	restoreDiff((Time*)decompedArray);

	tt1->array = decompedArray;
	decompressionCost += (srcLen - compressedSize);
	tt1->size = srcLen;

	//tArrayIsDiff(tt1->array, l_table->tArrayBackup[0]);

	Time *diffBuffer = (Time*)MemPoolAlloc();

	for(unsigned i = 1; i < LevelTable::MAX_LEVEL; ++i) {
		TimeTable* tt2 = l_table->tArray[i];
		TimeTable* ttPrev = l_table->tArray[i-1];
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

		int j;
		for(j = 0; j < TimeTable::TIMETABLE_SIZE/2; ++j) {
			assert(diffBuffer[j] >= 0);
			tt2->array[j] = ttPrev->array[j] - diffBuffer[j];

		}
	#if 0
		if (memcmp(tt2->array, l_table->tArrayBackup[i], uncompLen) != 0) {
			fprintf(stderr, "error at level %d\n", i);
			assert(0);
		}
	#endif
//		assert(memcmp(tt2->array, l_table->tArrayBackup[i], uncompLen) == 0);
		//tArrayIsDiff(tt2->array, l_table->tArrayBackup[i]);
	}

	MemPoolFree(diffBuffer);
	l_table->isCompressed = 0;
	return decompressionCost;
}


class ActiveSetEntry {
public:
	UInt16 r_bit;
	UInt32 code;
};

static std::map<LevelTable*, ActiveSetEntry*> active_set;
static std::map<LevelTable*, ActiveSetEntry*>::iterator active_set_clock_hand;

/*! \brief Move "clockhand" to next entry in active set */
static inline void advanceClockHand() {
	++active_set_clock_hand;
	if (active_set_clock_hand == active_set.end())
		active_set_clock_hand = active_set.begin();
}

/*! \brief Prints all entries in the active set.  */
static inline void printActiveSet() {
	unsigned i = 0;
	for(std::map<LevelTable*, ActiveSetEntry*>::iterator it = active_set.begin(); 
			it != active_set.end(); ++it, ++i) {
		if (it == active_set_clock_hand) fprintf(stderr,"*");
		fprintf(stderr,"%u: key = %p, r_bit = %hu\n", i, it->first, it->second->r_bit);
	}
}

/*! \brief Allocate memory for a new active set entry.
 *
 * \return The newly allocated active set entry.
 */
static inline ActiveSetEntry* ActiveSetEntryAlloc() {
	ActiveSetEntry *as = (ActiveSetEntry*)MemPoolAllocSmall(sizeof(ActiveSetEntry));
	as->r_bit = 1;
	as->code = 0xDEADBEEF;
	return as;
}

/*! \brief Deallocates (i.e. frees) memory for active set entry
 *
 * \param entry The active set entry to deallocate.
 */
static inline void ActiveSetEntryFree(ActiveSetEntry* entry) {
	MemPoolFreeSmall(entry, sizeof(ActiveSetEntry));
}

static int bufferSize;

void CBufferInit(int size) {
	if (KConfigGetCompression() == 0)
		return;

	fprintf(stderr,"Initializing compression buffer to size %d\n",size);
	
	if (lzo_init() != LZO_E_OK) {
		printf("internal error - lzo_init() failed !!!\n");
	    printf("(this usually indicates a compiler bug - try recompiling\nwithout optimizations, and enable '-DLZO_DEBUG' for diagnostics)\n");
	    return;
	}

	bufferSize = size;
}

extern UInt64 _compSrcSize, _compDestSize;

void CBufferDeinit() {
	fprintf(stderr, "CBuffer (evict / access / ratio) = %lld, %lld, %.2f \%\n",
		totalEvict, totalAccess, ((double)totalEvict / totalAccess) * 100.0);
	fprintf(stderr, "Compression Overall Rate = %.2f X\n", (double)_compSrcSize / _compDestSize);
}

/*! \brief Find an entry to remove from active set.
 *
 * \return The active set entry that should be removed.
 * \remark This simply returns an entry that should be removed. It does not
 * actually remove the entry.
 */
std::map<LevelTable*, ActiveSetEntry*>::iterator getVictim() {
	// set active_set_clock_hand to entry that will be removed
	while(active_set_clock_hand->second->r_bit == 1) {
		active_set_clock_hand->second->r_bit = 0;
		assert(active_set_clock_hand->second->code == 0xDEADBEEF);
		assert(active_set_clock_hand->first->code == 0xDEADBEEF);
		advanceClockHand();
	}

	assert(active_set_clock_hand->first->code == 0xDEADBEEF);
	assert(active_set_clock_hand->second->code == 0xDEADBEEF);
	std::map<LevelTable*, ActiveSetEntry*>::iterator ret = active_set_clock_hand;
	advanceClockHand();
	return ret;
}


/*! \brief Adds a level table entry to the compression buffer.
 *
 * \param l_table The level table to add to the buffer.
 */
static inline void addToBuffer(LevelTable* l_table) {
	//fprintf(stderr, "adding l_table 0x%llx\n", l_table);
	ActiveSetEntry *as = ActiveSetEntryAlloc();
	active_set.insert( std::pair<LevelTable*, ActiveSetEntry*>(l_table, as) );

	// TRICKY: size will only be 1 the first time we add something to the
	// buffer so we'll go ahead and initialize the clock hand to that first
	// entry
	if (active_set.size() == 1) active_set_clock_hand = active_set.begin();
}

/*! \brief Removes a suitable entry from the compression buffer.
 *
 * \return The number of bytes saved by removing from the buffer.
 */
static inline int evictFromBuffer() {
	std::map<LevelTable*, ActiveSetEntry*>::iterator victim = getVictim();
	assert(victim->second->code == 0xDEADBEEF);
	LevelTable* lTable = victim->first;
	assert(lTable->code == 0xDEADBEEF);
	int bytes_gained = compressLevelTable(lTable);
	assert(lTable->code == 0xDEADBEEF);
	totalEvict++;
	ActiveSetEntryFree(victim->second);
	active_set.erase(victim);
	return bytes_gained;
}

int CBufferDecompress(LevelTable* table) {
	int loss = decompressLevelTable(table);
	int gain = CBufferAdd(table);
	return gain - loss;
}

int CBufferAdd(LevelTable* table) {
	assert(table->code == 0xDEADBEEF);
	if (KConfigGetCompression() == 0)
		return 0;

	//fprintf(stderr,"adding %p to active set\n",table);
	int bytes_gained = 0;
	if(active_set.size() >= bufferSize) {
		bytes_gained = evictFromBuffer();
	}

	addToBuffer(table);
	return bytes_gained;
}

void CBufferAccess(LevelTable* table) {
	if (KConfigGetCompression() == 0)
		return;

	std::map<LevelTable*, ActiveSetEntry*>::iterator it = active_set.find(table);
	if (it == active_set.end()) {
		fprintf(stderr, "[1] as not found for lTable 0x%llx\n", table);
	}
	assert(it != active_set.end());
	ActiveSetEntry *as = it->second;
	assert(as->code == 0xDEADBEEF);
	as->r_bit = 1;
	totalAccess++;
}

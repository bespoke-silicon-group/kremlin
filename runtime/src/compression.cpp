#include <map>

#include "kremlin.h"
#include "compression.h"
#include "MShadowSkadu.h"
#include "config.h"
#include "minilzo.h"

static UInt64 _compSrcSize;
static UInt64 _compDestSize;
static UInt64 totalAccess;
static UInt64 totalEvict;

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);


UInt8* compressData(UInt8* src, lzo_uint src_size, lzo_uintp dest_size) {
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

void decompressData(UInt8* dest, UInt8* src, lzo_uint src_size, lzo_uintp dest_size) {
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

void CBuffer::init(int size) {
	if (KConfigGetCompression() == 0)
		return;

	fprintf(stderr,"Initializing compression buffer to size %d\n",size);
	
	if (lzo_init() != LZO_E_OK) {
		printf("internal error - lzo_init() failed !!!\n");
	    printf("(this usually indicates a compiler bug - try recompiling\nwithout optimizations, and enable '-DLZO_DEBUG' for diagnostics)\n");
	    return;
	}

	this->num_entries = size;
}

void CBuffer::deinit() {
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

void CBuffer::addToBuffer(LevelTable* l_table) {
	//fprintf(stderr, "adding l_table 0x%llx\n", l_table);
	ActiveSetEntry *as = ActiveSetEntryAlloc();
	active_set.insert( std::pair<LevelTable*, ActiveSetEntry*>(l_table, as) );

	// TRICKY: size will only be 1 the first time we add something to the
	// buffer so we'll go ahead and initialize the clock hand to that first
	// entry
	if (active_set.size() == 1) active_set_clock_hand = active_set.begin();
}

int CBuffer::evictFromBuffer() {
	std::map<LevelTable*, ActiveSetEntry*>::iterator victim = getVictim();
	assert(victim->second->code == 0xDEADBEEF);
	LevelTable* lTable = victim->first;
	assert(lTable->code == 0xDEADBEEF);
	int bytes_gained = lTable->compress();
	assert(lTable->code == 0xDEADBEEF);
	totalEvict++;
	ActiveSetEntryFree(victim->second);
	active_set.erase(victim);
	return bytes_gained;
}

int CBuffer::decompress(LevelTable* table) {
	int loss = table->decompress();
	int gain = this->add(table);
	return gain - loss;
}

int CBuffer::add(LevelTable* table) {
	assert(table->code == 0xDEADBEEF);
	if (KConfigGetCompression() == 0)
		return 0;

	//fprintf(stderr,"adding %p to active set\n",table);
	int bytes_gained = 0;
	if(active_set.size() >= this->num_entries) {
		bytes_gained = evictFromBuffer();
	}

	addToBuffer(table);
	return bytes_gained;
}

void CBuffer::touch(LevelTable* table) {
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

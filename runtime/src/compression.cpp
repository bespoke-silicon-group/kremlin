#include <map>

#include "kremlin.h"
#include "compression.hpp"
#include "ShadowMemory.hpp"
#include "LevelTable.hpp"
#include "config.hpp"
#include "minilzo.h"
#include "debug.hpp"

static uint64_t _compSrcSize;
static uint64_t _compDestSize;
static uint64_t totalAccess;
static uint64_t totalEvict;

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);


uint8_t* compressData(uint8_t* decomp_data, lzo_uint decomp_size, 
					lzo_uintp comp_size) {
	assert(decomp_data != nullptr);
	assert(decomp_size > 0);
	assert(comp_size != nullptr);

	//XXX need a specialized memory allocator, for now, just check the 

	//uint8_t *comp_data = MemPoolAlloc();
	uint8_t *comp_data = (uint8_t*)malloc(decomp_size);
	int result = lzo1x_1_compress(decomp_data, decomp_size, comp_data, comp_size, wrkmem);
	assert(result == LZO_E_OK);
	//memcpy(comp_data, decomp_data, decomp_size);
	//*comp_size  = decomp_size;

	//MSG(3, "compressed from %d to %d\n", decomp_size, *comp_size);
	_compSrcSize += decomp_size;
	_compDestSize += *comp_size;

	assert(comp_data != nullptr);
	return comp_data;
}

void decompressData(uint8_t* decomp_data, uint8_t* comp_data, 
					lzo_uint comp_size, lzo_uintp decomp_size) {
	assert(comp_data != nullptr);
	assert(decomp_data != nullptr);
	assert(comp_size > 0);
	assert(decomp_size != nullptr);

	int result = lzo1x_decompress(comp_data, comp_size, decomp_data, decomp_size, nullptr);
	assert(result == LZO_E_OK);
	//memcpy(decomp_data, comp_data, comp_size);
	//*decomp_size = comp_size;

	//MSG(3, "decompressed from %d to %d\n", comp_size, *decomp_size);
	//MemPoolFree(comp_data);
	free(comp_data);
	comp_data = nullptr;
	assert(comp_data == nullptr);
}

/*
 * BEGIN ACTIVE SET MAINTENANCE CODE.
 */

/*!
 * An entry in the active set of uncompressed level tables.
 */
class ActiveSetEntry {
public:
	uint16_t r_bit;
	uint32_t code; // TODO: make this debug mode only
	ActiveSetEntry() : r_bit(1), code(0xDEADBEEF) {}
	~ActiveSetEntry() {}

	void* operator new(size_t size) {
		return MemPoolAllocSmall(sizeof(ActiveSetEntry));
	}
	void operator delete(void *ptr) {
		MemPoolFreeSmall(ptr, sizeof(ActiveSetEntry));
	}
};

typedef std::map<LevelTable*, ActiveSetEntry*>::iterator active_set_iterator;

static std::map<LevelTable*, ActiveSetEntry*> active_set;
static active_set_iterator active_set_clock_hand;

/*! \brief Move "clockhand" to next entry in active set */
static inline void advanceClockHand() {
	++active_set_clock_hand;
	if (active_set_clock_hand == active_set.end())
		active_set_clock_hand = active_set.begin();
}

/*! \brief Prints all entries in the active set.  */
static inline void printActiveSet() {
	unsigned i = 0;
	for(active_set_iterator it = active_set.begin(); 
			it != active_set.end(); ++it, ++i) {
		if (it == active_set_clock_hand) MSG(3,"*");
		MSG(3,"%u: key = %p, r_bit = %hu\n", i, it->first, it->second->r_bit);
	}
}

CBuffer::CBuffer(unsigned size) {
	assert(size > 0);
	if (!kremlin_config.compressShadowMem()) return;

	MSG(2,"Initializing compression buffer to size %d\n",size);
	
	// TODO: make this an exception
	if (lzo_init() != LZO_E_OK) {
		printf("internal error - lzo_init() failed !!!\n");
	    printf("(this usually indicates a compiler bug - try recompiling\nwithout optimizations, and enable '-DLZO_DEBUG' for diagnostics)\n");
	    return;
	}

	this->num_entries = size;
}

CBuffer::~CBuffer() {
	MSG(2, "CBuffer (evict / access / ratio) = %llu, %llu, %.2f\n",
		totalEvict, totalAccess, ((double)totalEvict / totalAccess) * 100.0);
	MSG(2, "Compression Overall Rate = %.2f X\n", (double)_compSrcSize / _compDestSize);
}

/*! \brief Find an entry to remove from active set.
 *
 * \return The active set entry that should be removed.
 * \remark This simply returns an entry that should be removed. It does not
 * actually remove the entry.
 */
active_set_iterator getVictim() {
	// set active_set_clock_hand to entry that will be removed
	while(active_set_clock_hand->second->r_bit == 1) {
		active_set_clock_hand->second->r_bit = 0;
		// TODO: these asserts need to go bye-bye
		assert(active_set_clock_hand->second->code == 0xDEADBEEF);
		assert(active_set_clock_hand->first->code == 0xDEADBEEF);
		advanceClockHand();
	}

	// TODO: these asserts need to go bye-bye
	assert(active_set_clock_hand->first->code == 0xDEADBEEF);
	assert(active_set_clock_hand->second->code == 0xDEADBEEF);

	active_set_iterator ret = active_set_clock_hand;
	advanceClockHand();
	return ret;
}

void CBuffer::addToBuffer(LevelTable *l_table) {
	assert(l_table != nullptr);

	//fprintf(stderr, "adding l_table 0x%llx\n", l_table);
	ActiveSetEntry *as = new ActiveSetEntry();
	active_set.insert( std::pair<LevelTable*, ActiveSetEntry*>(l_table, as) );

	// TRICKY: size will only be 1 the first time we add something to the
	// buffer so we'll go ahead and initialize the clock hand to that first
	// entry
	if (active_set.size() == 1) active_set_clock_hand = active_set.begin();
	// TODO: assert active_size size is less than num_entries
}

int CBuffer::evictFromBuffer() {
	active_set_iterator victim = getVictim();
	assert(victim->second->code == 0xDEADBEEF);
	LevelTable* lTable = victim->first;
	int bytes_gained = lTable->compress();
	totalEvict++;
	delete victim->second;
	victim->second = nullptr;
	active_set.erase(victim);
	return bytes_gained;
}

int CBuffer::decompress(LevelTable *table) {
	assert(table != nullptr);

	int loss = table->decompress();
	int gain = this->add(table);
	return gain - loss;
}

int CBuffer::add(LevelTable *table) {
	assert(table != nullptr);
	assert(table->code == 0xDEADBEEF);

	if (!kremlin_config.compressShadowMem()) return 0;

	int bytes_gained = 0;
	// XXX: is next line really >=. Why not just >? (-sat)
	if(active_set.size() >= this->num_entries) {
		bytes_gained = evictFromBuffer();
	}

	addToBuffer(table);
	return bytes_gained;
}

void CBuffer::touch(LevelTable *table) {
	assert(table != nullptr);
	if (!kremlin_config.compressShadowMem()) return;

	active_set_iterator it = active_set.find(table);
	if (it == active_set.end()) {
		fprintf(stderr, "[1] as not found for lTable 0x%p\n", table);
	}
	assert(it != active_set.end());
	ActiveSetEntry *as = it->second;
	assert(as->code == 0xDEADBEEF);
	as->r_bit = 1;
	totalAccess++;
}

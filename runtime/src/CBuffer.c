#include "kremlin.h"
#include "CBuffer.h"
#include "MShadowLow.h"
#include "compress.h"
#include "uthash.h"
#include "config.h"
#include "minilzo.h"

static UInt64 totalAccess;
static UInt64 totalEvict;

typedef struct _ActiveSetEntry {
	LTable* key;
	UInt16 r_bit;
	UT_hash_handle hh;
	UInt32 code;
} ASEntry;

static ASEntry* activeSet = NULL;
static ASEntry* activeSetClockHand = NULL;

UInt32 numInActiveSet = 0;

static inline void advanceClockHand() {
	activeSetClockHand = activeSetClockHand->hh.next;
	if(activeSetClockHand == NULL) 
		activeSetClockHand = activeSet;
}

static inline void printActiveSet() {
	ASEntry* as;

	int i = 0;
	for(as = activeSet; as != NULL; as = as->hh.next, ++i) {
		fprintf(stderr,"%d: key = %p, r_bit = %hu\n",i,as->key,as->r_bit);
	}
}

static inline ASEntry* ASEntryAlloc(LTable* lTable) {
	ASEntry *as = MemPoolAllocSmall(sizeof(ASEntry));
	as->key = lTable;
	as->r_bit = 1;
	as->code = 0xDEADBEEF;
	return as;
}

static inline void ASEntryFree(ASEntry* entry) {
	MemPoolFreeSmall(entry, sizeof(ASEntry));
}

static int bufferSize;
void CBufferInit(int size) {
	if (getCompression() == 0)
		return;
	
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

ASEntry* getVictim() {
	// set activeSetClockHand to entry that will be removed
	while(activeSetClockHand->r_bit == 1) {
		activeSetClockHand->r_bit = 0;
		assert(activeSetClockHand->code == 0xDEADBEEF);
		assert(activeSetClockHand->key->code == 0xDEADBEEF);
		advanceClockHand();
	}
	assert(activeSetClockHand->code == 0xDEADBEEF);
	assert(activeSetClockHand->key->code == 0xDEADBEEF);
	ASEntry* ret = activeSetClockHand;
	advanceClockHand();
	return ret;
}


static inline void addToBuffer(LTable* lTable) {
	//fprintf(stderr, "adding lTable 0x%llx\n", lTable);
	ASEntry *as = ASEntryAlloc(lTable);
	HASH_ADD_PTR(activeSet, key, as);
	numInActiveSet++;

	ASEntry *as2 = NULL;
	HASH_FIND_PTR(activeSet, &lTable, as2);
	assert(as2 != NULL);
	if (as2 == NULL) {
		fprintf(stderr, "[0] as not found for lTable 0x%llx\n", lTable);
	}

	if(numInActiveSet == 1) 
		activeSetClockHand = activeSet;
	return;
}

static inline int evictFromBuffer() {
	numInActiveSet--;

	ASEntry* victim = getVictim();
	assert(victim->code == 0xDEADBEEF);
	LTable* lTable = victim->key;
	//fprintf(stderr, "\tevicting lTable 0x%llx\n", lTable);
	assert(lTable->code == 0xDEADBEEF);
	int sizeGained = compressLTable(lTable);
	assert(lTable->code == 0xDEADBEEF);
	totalEvict++;
	HASH_DEL(activeSet, victim);
	ASEntryFree(victim);
	return sizeGained;
}

int CBufferAdd(LTable* lTable) {
	assert(lTable->code == 0xDEADBEEF);
	int sizeGained = 0;
	if (getCompression() == 0)
		return 0;

	//fprintf(stderr,"adding %p to active set\n",lTable);
	if(numInActiveSet >= bufferSize) {
		sizeGained = evictFromBuffer();
	}

	addToBuffer(lTable);
	return sizeGained;
}

void CBufferAccess(LTable* lTable) {
	if (getCompression() == 0)
		return;

	ASEntry *as;
	HASH_FIND_PTR(activeSet, &lTable, as);
	if (as == NULL) {
		fprintf(stderr, "[1] as not found for lTable 0x%llx\n", lTable);
	}
	assert(as != NULL);
	assert(as->code == 0xDEADBEEF);
	as->r_bit = 1;
	totalAccess++;
}

#if 0
// from now on, direct mapped cache implementation
typedef struct _c_entry {
	LTable* table;
} CEntry;

static CEntry* buffer;
static int bufferSize = 1024;
static int bufferMask;


static inline int getIndex(LTable* table) {
	int index1 = ((UInt64)table >> 4) & bufferMask;
	int index2 = ((UInt64)table >> (4 + 14)) & bufferMask;
	assert(ret < bufferSize);
	return index1 ^ index2;
}

static int CBufferEvict(LTable* table) {
	//fprintf(stderr, "evicting 0x%x\n", table);
	return compressLTable(table);
}

void CBufferInit(int size) {
	bufferSize = size;
	bufferMask = size - 1;
	buffer = calloc(sizeof(CEntry), bufferSize);
}

void CBufferDeinit() {
	free(buffer);
}


#if 0
int CBufferAdd(LTable* table) {
	//fprintf(stderr, "CBufferAdd\n", table);
	int index = getIndex(table);
	int ret = 0;
	LTable* prev = buffer[index].table;	
	if (prev != NULL) {
		ret = CBufferEvict(prev);
	}
	buffer[index].table = table;
	return ret;
}
#endif
#endif


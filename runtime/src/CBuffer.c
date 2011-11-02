#include "defs.h"
#include "CBuffer.h"
#include "MShadowLow.h"
#include "compress.h"
#include "uthash.h"
#include "config.h"

static UInt64 totalAccess;
static UInt64 totalEvict;

typedef struct _ActiveSetEntry {
	LTable* key;
	UInt16 r_bit;
	UT_hash_handle hh;
} ASEntry;

static ASEntry* activeSet = NULL;
static ASEntry* activeSetClockHand = NULL;

UInt32 numInActiveSet = 0;

static inline void advanceActiveSetClockHand() {
	activeSetClockHand = activeSetClockHand->hh.next;
	if(activeSetClockHand == NULL) activeSetClockHand = activeSet;
}

static inline void printActiveSet() {
	ASEntry* as;

	int i = 0;
	for(as = activeSet; as != NULL; as = as->hh.next, ++i) {
		fprintf(stderr,"%d: key = %p, r_bit = %hu\n",i,as->key,as->r_bit);
	}
}

static inline ASEntry* createNewASEntry(LTable* lTable) {
	ASEntry *as = malloc(sizeof(ASEntry));
	as->key = lTable;
	as->r_bit = 1;
	return as;
}

static int bufferSize;
void CBufferInit(int size) {
	bufferSize = size;
}

void CBufferDeinit() {
	fprintf(stderr, "CBuffer (evict / access / ratio) = %lld, %lld, %.2f \%\n",
		totalEvict, totalAccess, ((double)totalEvict / totalAccess) * 100.0);
}


int CBufferAdd(LTable* lTable) {
	if (getCompression() == 0)
		return 0;

	//fprintf(stderr,"adding %p to active set\n",lTable);
	// just add this if we are less than the buffer size
	if(numInActiveSet < bufferSize) {
		ASEntry *as = createNewASEntry(lTable);
		HASH_ADD_PTR(activeSet,key,as);
		numInActiveSet++;

		if(numInActiveSet == 1) activeSetClockHand = activeSet;
		return 0;
	}

	// set activeSetClockHand to entry that will be removed
	while(activeSetClockHand->r_bit == 1) {
		activeSetClockHand->r_bit = 0;
		advanceActiveSetClockHand();
	}

	/*
	if(lTable == activeSetClockHand->key) {
		fprintf(stderr,"ERROR: evicting same thing we are putting in???\n");
	}
	*/

	//fprintf(stderr,"evicting %p from active set\n",activeSetClockHand->key);
	//fprintf(stderr,"evicting %p from active set (checked %d entries for 0 r bit)\n",activeSetClockHand->key,numCheckForZeroRBit);

	// compress the entry to be removed
	int ret = compressLTable(activeSetClockHand->key);
	totalEvict++;

	// add lTable to active set
	ASEntry *as = createNewASEntry(lTable);
	HASH_ADD_PTR(activeSet,key,as);
	HASH_DEL(activeSet,activeSetClockHand);
	//activeSetClockHand->key = lTable;
	//activeSetClockHand->r_bit = 1;

	advanceActiveSetClockHand();
	//printActiveSet();
	return ret;
}

void CBufferAccess(LTable* lTable) {
	if (getCompression() == 0)
		return;

	ASEntry *as;
	HASH_FIND_PTR(activeSet,&lTable,as);
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


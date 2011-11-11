#include "kremlin.h"
#include "CBuffer.h"
#include "MShadowLow.h"
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


static UInt8* compressData(UInt8* src, lzo_uint sizeSrc, lzo_uintp sizeDest) {
	assert(src != NULL);
	assert(sizeSrc > 0);
	assert(sizeDest != NULL);

	//XXX need a specialized memory allocator, for now, just check the 

	UInt8* dest = MemPoolAlloc();
	int result = lzo1x_1_compress(src, sizeSrc, dest, sizeDest, wrkmem);
	assert(result == LZO_E_OK);
	//memcpy(dest, src, sizeSrc);
	//*sizeDest  = sizeSrc;

	//fprintf(stderr, "compressed from %d to %d\n", sizeSrc, *sizeDest);
	_compSrcSize += sizeSrc;
	_compDestSize += *sizeDest;
	return dest;
}

static void decompressData(UInt8* dest, UInt8* src, lzo_uint sizeSrc, lzo_uintp sizeDest) {
	assert(src != NULL);
	assert(dest != NULL);
	assert(sizeSrc > 0);
	assert(sizeDest != NULL);

	int result = lzo1x_decompress(src, sizeSrc, dest, sizeDest, NULL);
	assert(result == LZO_E_OK);
	//memcpy(dest, src, sizeSrc);
	//*sizeDest = sizeSrc;

	//fprintf(stderr, "decompressed from %d to %d\n", sizeSrc, *sizeDest);
	MemPoolFree(src);
}

void makeDiff(Time* array) {
	int size = TIMETABLE_SIZE / 2;
	int i;

	for (i=size-1; i>=1; i--) {
		array[i] = array[i] - array[i-1];
	}
}

void restoreDiff(Time* array) {
	int size = TIMETABLE_SIZE / 2;
	int i;

	for (i=1; i<size; i++) {
		array[i] += array[i-1];
	}
}

int getTimeTableSize(LTable* lTable) {
	int i;
	for (i=0; i<MAX_LEVEL; i++) {
		TimeTable* table = lTable->tArray[i];
		if (table == NULL)
			return i;
	}
	assert(0);
	return -1;
}

// we'll assume you already GC'ed lTable... otherwise you are going to be
// doing useless work (i.e. compressing data that is out of date)
// Returns: number of bytes saved by compression
static UInt64 compressLTable(LTable* lTable) {
	//fprintf(stderr,"[LTable] compressing LTable (%p)\n",lTable);
	if (lTable->code != 0xDEADBEEF) {
		fprintf(stderr, "LTable addr = 0x%llx\n", lTable);
		assert(0);
	}
	assert(lTable->code == 0xDEADBEEF);
	assert(lTable->isCompressed == 0);

	TimeTable* tt1 = lTable->tArray[0];

	if (tt1 == NULL) {
		lTable->isCompressed = 1;
		return 0;
	}


	UInt64 compressionSavings = 0;
	lzo_uint srcLen = sizeof(Time)*TIMETABLE_SIZE/2; // XXX assumes 8 bytes
	lzo_uint compLen = 0;

	int i;
	Time* diffBuffer = MemPoolAlloc();
	void* compressedData;
#if 1
	for(i = MAX_LEVEL-1; i >=1; i--) {
		// step 1: create/fill in time difference table
		TimeTable* tt2 = lTable->tArray[i];
		TimeTable* ttPrev = lTable->tArray[i-1];
		if(tt2 == NULL)
			continue;

		assert(tt2 != NULL);
		assert(ttPrev != NULL);

		lTable->tArrayBackup[i] = tt2->array;

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			diffBuffer[j] = ttPrev->array[j] - tt2->array[j];
		}

		// step 2: compress diffs
		makeDiff(diffBuffer);
		compressedData = compressData((UInt8*)diffBuffer, srcLen, &compLen);
		compressionSavings += (srcLen - compLen);
		tt2->size = compLen;

		// step 3: profit
		//MemPoolFree(tt2->array); // XXX: comment this out if using tArrayBackup
		tt2->array = compressedData;
	}
	Time* level0Array = MemPoolAlloc();
	memcpy(level0Array, tt1->array, srcLen);
	makeDiff(tt1->array);
	compressedData = compressData((UInt8*)tt1->array, srcLen, &compLen);
	MemPoolFree(tt1->array);
	//Time* level0Array = tt1->array;
	tt1->array = compressedData;
	tt1->size = compLen;
	compressionSavings += (srcLen - compLen);

	// only for result checking
	lTable->tArrayBackup[0] = level0Array;


#endif

	//MemPoolFree(level0Array);  // XXX: comment this out if using tArrayBackup
	MemPoolFree(diffBuffer);

	lTable->isCompressed = 1;
	return compressionSavings;
}


static UInt64 decompressLTable(LTable* lTable) {

	if (lTable->code != 0xDEADBEEF) {
		fprintf(stderr, "LTable addr = 0x%llx\n", lTable);
		assert(0);
	}
	assert(lTable->code == 0xDEADBEEF);
	assert(lTable->isCompressed == 1);

	//fprintf(stderr,"[LTable] decompressing LTable (%p)\n",lTable);
	UInt64 decompressionCost = 0;
	lzo_uint srcLen = sizeof(Time)*TIMETABLE_SIZE/2;
	lzo_uint uncompLen = srcLen;

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = lTable->tArray[0];
	if (tt1 == NULL) {
		lTable->isCompressed = 0;
		return 0;
	}
	int compressedSize = tt1->size;

	Time* decompedArray = MemPoolAlloc();
	decompressData((UInt8*)decompedArray, (UInt8*)tt1->array, compressedSize, &uncompLen);
	restoreDiff((Time*)decompedArray);

	tt1->array = decompedArray;
	decompressionCost += (srcLen - compressedSize);
	tt1->size = srcLen;

	//tArrayIsDiff(tt1->array, lTable->tArrayBackup[0]);

	int i;
	Time *diffBuffer = MemPoolAlloc();
#if 1
	for(i = 1; i < MAX_LEVEL; ++i) {
		TimeTable* tt2 = lTable->tArray[i];
		TimeTable* ttPrev = lTable->tArray[i-1];
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
		tt2->array = MemPoolAlloc();
		tt2->size = srcLen;

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			assert(diffBuffer[j] >= 0);
			tt2->array[j] = ttPrev->array[j] - diffBuffer[j];

		}
	#if 0
		if (memcmp(tt2->array, lTable->tArrayBackup[i], uncompLen) != 0) {
			fprintf(stderr, "error at level %d\n", i);
			assert(0);
		}
	#endif
//		assert(memcmp(tt2->array, lTable->tArrayBackup[i], uncompLen) == 0);
		//tArrayIsDiff(tt2->array, lTable->tArrayBackup[i]);
	}
#endif


	MemPoolFree(diffBuffer);
	lTable->isCompressed = 0;
	return decompressionCost;
}


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
	if (KConfigGetCompression() == 0)
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

int CBufferDecompress(LTable* lTable) {
	int loss = decompressLTable(lTable);
	int gain = CBufferAdd(lTable);
	return gain - loss;
}

int CBufferAdd(LTable* lTable) {
	assert(lTable->code == 0xDEADBEEF);
	int sizeGained = 0;
	if (KConfigGetCompression() == 0)
		return 0;

	//fprintf(stderr,"adding %p to active set\n",lTable);
	if(numInActiveSet >= bufferSize) {
		sizeGained = evictFromBuffer();
	}

	addToBuffer(lTable);
	return sizeGained;
}

void CBufferAccess(LTable* lTable) {
	if (KConfigGetCompression() == 0)
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


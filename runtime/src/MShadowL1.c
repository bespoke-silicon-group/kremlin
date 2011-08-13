#include "defs.h"
#include "debug.h"
#include "Table.h"
#include "RShadow.h"

#define INIT_LEVEL		16	// max index depth
#define WORD_SIZE_SHIFT 2	// 32bit word
#define LINE_SIZE_SHIFT	3
#define LINE_SIZE		(1 << LINE_SIZE_SHIFT)
#define LINE_SIZE_MASK	(LINE_SIZE - 1)

#define NUM_LINE_SHIFT	12
#define NUM_LINE		(1 << NUM_LINE_SHIFT)
#define NUM_LINE_MASK	(NUM_LINE - 1)

#define STATUS_VALID	1
#define STATUS_DIRTY	2

typedef struct _L1Stat {
	int nRead;
	int nReadHit;
	int nReadEvict;
	int nWrite;
	int nWriteHit;
	int nWriteEvict;
} L1Stat;

L1Stat stat;

static inline void eventRead() {
	stat.nRead++;
}

static inline void eventReadHit() {
	stat.nReadHit++;
}

static inline void eventReadEvict() {
	stat.nReadEvict++;
}

static inline void eventWrite() {
	stat.nWrite++;
}

static inline void eventWriteHit() {
	stat.nWriteHit++;
}

static inline void eventWriteEvict() {
	stat.nWriteEvict++;
}

static inline void printStat() {
	fprintf(stderr, "L1 Cache Stat\n");	
	fprintf(stderr, "read  all / hit / evict = %d / %d / %d\n", stat.nRead, stat.nReadHit, stat.nReadEvict);
	fprintf(stderr, "write all / hit / evict = %d / %d / %d\n", stat.nWrite, stat.nWriteHit, stat.nWriteEvict);
	double hit = (stat.nReadHit + stat.nWriteHit) * 100.0 / (stat.nRead + stat.nWrite);
	fprintf(stderr, "overall hit ratio = %.2f\n", hit);
}


void MShadowSetFromCache(Addr addr, Index size, Version* vArray, Time* tArray);

// addr[1:0] word offset
// addr[4:2] line offset (8 words / line)
// addr[11:5] index

typedef struct _L1Entry {
	UInt64 tag;  // addr[63:5]
	UInt64 expire;
	UInt32 status;	// possible to merge with tag field

} L1Entry;

typedef struct _MShadowL1 {
	L1Entry entry[NUM_LINE];
	
} MShadowL1;

static MShadowL1 tagTable;
static LTable* valueTable;

static inline Bool isValid(L1Entry* entry) {
	return entry->status & STATUS_VALID;
}

static inline Bool isDirty(L1Entry* entry) {
	return entry->status & (STATUS_DIRTY | STATUS_VALID);
}

static inline void setValid(L1Entry* entry) {
	entry->status |= STATUS_VALID;
}

static inline void setDirty(L1Entry* entry) {
	entry->status |= STATUS_DIRTY;
}

static inline void resetDirty(L1Entry* entry) {
	entry->status &= ~STATUS_DIRTY;
}

static inline Bool hasExpired(L1Entry* entry, Index index) {
	return entry->expire & (1 << index);
}

static inline void setExpired(L1Entry* entry, Index index) {
	entry->expire | (1 << index);
}

static inline void clearExpired(L1Entry* entry, Index index) {
	entry->expire & ~(1 << index);
}

static inline int getTag(Addr addr) {
	int nShift = WORD_SIZE_SHIFT + LINE_SIZE_SHIFT + NUM_LINE_SHIFT;
	UInt64 mask = ~((1 << nShift) - 1);
	return (UInt64)addr & mask;
}

static inline void setTag(L1Entry* entry, Addr addr) {
	entry->tag = getTag(addr);
}

static inline Bool isHit(L1Entry* entry, Addr addr) {
	MSG(0, "isHit addr = 0x%llx, tag = 0x%llx, entry tag = 0x%llx\n",
		addr, getTag(addr), entry->tag);

	return isValid(entry) && (entry->tag == getTag(addr));
}

static inline Bool requiresEviction(L1Entry* entry, Addr addr) {
	return !isHit(entry, addr) && isDirty(entry); 
}

static inline Time* getTimeAddr(int row, int col) {
	return TableGetElementAddr(valueTable, row, col * INIT_LEVEL);
}

static inline Addr getLineBaseAddr(Addr addr) {
	int mask = 1 << (WORD_SIZE_SHIFT + LINE_SIZE_SHIFT);
	return (Addr)((UInt64)addr & ~(mask - 1));
}

// what's the line index in cache?
static inline int getLineIndex(Addr addr) {
	int nShift = WORD_SIZE_SHIFT + LINE_SIZE_SHIFT;
	int ret = (((UInt64)addr) >> nShift) & NUM_LINE_MASK;
	assert(ret >= 0 && ret < NUM_LINE);
	return ret;
}

// what's the word offset in a line? 
static inline int getLineOffset(Addr addr) {
	int nShift = WORD_SIZE_SHIFT;
	int ret = (((UInt64)addr) >> nShift) & LINE_SIZE_MASK;
	return ret;
}

static inline L1Entry* getEntry(int index) {
	assert(index < NUM_LINE);
	return &(tagTable.entry[index]);
}

void MShadowL1Init() {
	int i = 0;
	for (i = 0; i < NUM_LINE; i++) {
		tagTable.entry[i].status = 0x0;
		tagTable.entry[i].tag = 0x0;
	}
	valueTable = TableCreate(NUM_LINE, INIT_LEVEL * LINE_SIZE);
	MSG(0, "MShadowL1Init: value Table created row %d col %d at addr 0x%x\n", 
		NUM_LINE, INIT_LEVEL * LINE_SIZE, valueTable->array);
}

void MShadowL1Deinit() {
	int i = 0;
	printStat();
}

void MShadowLineRefresh(L1Entry* entry, int row, int maxIndex) {
	int index;
	int col;

	for (index=maxIndex; index>=0; index--) {
		if (hasExpired(entry, index)) {
			for (col=0; col<LINE_SIZE; col++) {
				Time* tArray = getTimeAddr(row, col);
				tArray[index] = 0ULL;
			}
			clearExpired(entry, index);
		} else {
			break;
		}
	}
}

L1Entry* MShadowL1Evict(Addr addr, int row, int size, Version* vArray) {
	MSG(0, "MShadowL1Evict 0x%llx, row=%d size=%d vArray = 0x%llx\n", 
		addr, row, size, vArray);

	L1Entry* entry = getEntry(row);
	MShadowLineRefresh(entry, row, size-1);
		
	// copy timestamps to the target TimeTable
	// if 2nd cache is used, different scheme required		
	int i;
	for (i=0; i<LINE_SIZE; i++) {
		Time* tArray = getTimeAddr(row, i);
		MShadowSetFromCache(addr, size, vArray, tArray);
	}
	return entry;
}

#if 0
Time* MShadowL1Get(Addr addr, int row, int col) {
	MSG(0, "MShadowL1Get 0x%llx, row %d, col %d\n", 
		addr, row, col);

	L1Entry* entry = getEntry(row);

	if (isValid(entry) && isHit(entry, addr)) {
		MSG(0, "\t Cache Hit\n");
		// cache hit
		Time* ret = TableGetElementAddr(valueTable, row, col);
		assert(ret != NULL);
		return ret;	

	} else {  
		// cache miss		
		MSG(0, "\t Cache Miss\n");
		return NULL;
	}
}
#endif

#if 0
void MShadowL1Set(Addr addr, Index size, Time* tArray) {
	Time* array = MShadowL1Get(addr, size);	
	int i;
	for (i=0; i<size; i++) {
		array[i] = tArray[i];
	}
}
#endif

// when a new region enters, 
// the timestamp from the previous regions must be set to "0"
// MShadowRefresh is called from logRegionExit 
// so that it sets time stamp to "0"

void MShadowL1Refresh(Index index) {
	int i, j;
	for (i=0; i<NUM_LINE; i++) {
		L1Entry* entry = getEntry(i);
		setExpired(entry, index);
#if 0
		if (isDirty(entry)) {
			for (j=0; j<LINE_SIZE; j++) {
				Time* destAddr = getTimeAddr(i, j);
				destAddr[index] = 0ULL;
			}
		}
#endif
	}
}

void MShadowFetchLine(L1Entry* entry, Addr addr, Index size, Version* vArray) {
	int col, index;
	int row = getLineIndex(addr);
	Time* baseAddr = (Time*) getLineBaseAddr(addr);
	for (col=0; col<LINE_SIZE; col++) {
		Time* destAddr = getTimeAddr(row, col);
		for (index=0; index<size; index++) {
			*(destAddr + index) = MShadowGetTime(baseAddr + col, index, vArray[index]);
		}
	}

	entry->tag = getTag(addr);
	setValid(entry);
	resetDirty(entry);
}

Time* MShadowGet(Addr addr, Index size, Version* vArray) {

#if 1
	int row = getLineIndex(addr);
	int col = getLineOffset(addr);
	MSG(0, "MShadowGet 0x%llx, size %d vArray = 0x%llx \n", addr, size, vArray);
	MSG(0, "\t row = %d col = %d\n", row, col);
	eventRead();

	L1Entry* entry = getEntry(row);
	Time* destAddr = getTimeAddr(row, col);
	if (isHit(entry, addr)) {
		eventReadHit();
		MSG(0, "\t cache hit at 0x%llx \n", destAddr);
		MSG(0, "\t value0 %d value1 %d \n", destAddr[0], destAddr[1]);
		MShadowLineRefresh(entry, row, size-1);
		return destAddr;
	}
	
	// Unfortunately, this access results in a miss
	// 1. evict a line	
	if (requiresEviction(entry, addr)) {
		MSG(0, "\t eviction required \n", destAddr);
		eventReadEvict();
		MShadowL1Evict(addr, row, size, vArray);
	}

	
	// 2. read line from MShadow to the evicted line
	int i;
	MSG(0, "\t write values to cache \n", destAddr);
	MShadowFetchLine(entry, addr, size, vArray);
	return destAddr;
#endif
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray) {
	MSG(0, "MShadowSet 0x%llx, size %d vArray = 0x%llx tArray = 0x%llx\n", addr, size, vArray, tArray);
	int row = getLineIndex(addr);
	int col = getLineOffset(addr);
	MSG(0, "\t row = %d col = %d\n", row, col);
	eventWrite();

	L1Entry* entry = getEntry(row);
	Time* destAddr = getTimeAddr(row, col);

	assert(row < NUM_LINE);

	if (isHit(entry, addr)) {
		eventWriteHit();

	} else {
		if (requiresEviction(entry, addr)) {
			MSG(0, "\t eviction required\n", row, col);
			eventWriteEvict();
			MShadowL1Evict(addr, row, size, vArray);
		}
		// bring the cache line 
		MShadowFetchLine(entry, addr, size, vArray);
	} 		

	// write tArray values to cache line
	MSG(0, "\t write %d elements to cache at 0x%llx\n", size, destAddr);
	int i;
	for (i=0; i<size; i++) {
		*(destAddr + i) = tArray[i];
	}
	setDirty(entry);
}


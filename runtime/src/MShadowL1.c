#include "defs.h"

#ifndef USE_MSHADOW_BASE

#include "debug.h"
#include "Table.h"
#include "RShadow.h"

//#define BYPASS_CACHE
#define CACHE_WITH_VERSION

#define INIT_LEVEL		16	// max index depth
#define WORD_SIZE_SHIFT 2	// 32bit word

#if 0
#define LINE_SIZE_SHIFT	3
#define LINE_SIZE		(1 << LINE_SIZE_SHIFT)
#define LINE_SIZE_MASK	(LINE_SIZE - 1)
#endif

#define NUM_LINE_SHIFT	10
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


// forward declarations
void MShadowSetFromCache(Addr addr, Index size, Version* vArray, Time* tArray);
void MShadowSetTimeEvict(Addr addr, Index index, Version version, Time time);

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
static Table* valueTable;
static Table* versionTable;

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
	MSG(0, "\texpire word = 0x%x\n", entry->expire);
	return entry->expire & (1 << index);
}

static inline void setExpired(L1Entry* entry, Index index) {
	UInt64 mask = ~(((UInt64)1 << index) - 1);
	//entry->expire |= (1 << index);
	entry->expire |= mask;
}

static inline void clearExpired(L1Entry* entry, Index index) {
	entry->expire &= ~(1 << index);
}

static inline UInt64 getTag(Addr addr) {
	int nShift = WORD_SIZE_SHIFT + NUM_LINE_SHIFT;
	UInt64 mask = ~((1 << nShift) - 1);
	return (UInt64)addr & mask;
}

static inline void setTag(L1Entry* entry, Addr addr) {
	entry->tag = getTag(addr);
}

static inline Time* getTimeAddr(int row, int index) {
	return TableGetElementAddr(valueTable, row, index);
}

static inline Version* getVersionAddr(int row, int index) {
	return TableGetElementAddr(versionTable, row, index);
}

static inline Version getVersion(int row, int index) {
	return *TableGetElementAddr(versionTable, row, index);
}

static inline Addr getLineBaseAddr(Addr addr) {
	int mask = 1 << (WORD_SIZE_SHIFT);
	return (Addr)((UInt64)addr & ~(mask - 1));
}

// what's the line index in cache?
static inline int getLineIndex(Addr addr) {
	int nShift = WORD_SIZE_SHIFT;
	int ret = (((UInt64)addr) >> nShift) & NUM_LINE_MASK;
	assert(ret >= 0 && ret < NUM_LINE);
	return ret;
}

static inline Bool matchVersion(Addr addr, Index index, Version version) {
	int row = getLineIndex(addr);
	return getVersion(row, index) == version;
}

static inline void setVersion(Addr addr, Index index, Version version) {
	int row = getLineIndex(addr);
	*TableGetElementAddr(versionTable, row, index) = version;
}

static inline void setTimestamp(Addr addr, Index index, Time time) {
	int row = getLineIndex(addr);
	*TableGetElementAddr(valueTable, row, index) = time;
}

static inline Bool isHit(L1Entry* entry, Addr addr) {
	MSG(0, "isHit addr = 0x%llx, tag = 0x%llx, entry tag = 0x%llx\n",
		addr, getTag(addr), entry->tag);

	return isValid(entry) && (entry->tag == getTag(addr));
}

// what's the word offset in a line? 
#if 0
static inline int getLineOffset(Addr addr) {
	int nShift = WORD_SIZE_SHIFT;
	int ret = (((UInt64)addr) >> nShift);
	return ret;
}
#endif

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
	valueTable = TableCreate(NUM_LINE, INIT_LEVEL);
	versionTable = TableCreate(NUM_LINE, INIT_LEVEL);

	MSG(0, "MShadowL1Init: value Table created row %d col %d at addr 0x%x\n", 
		NUM_LINE, INIT_LEVEL, valueTable->array);
}

void MShadowL1Deinit() {
	int i = 0;
	printStat();
	TableFree(valueTable);
	TableFree(versionTable);
}

L1Entry* MShadowL1Evict(Addr addr, int row, int size, Version* vArray) {
	MSG(0, "MShadowL1Evict 0x%llx, row=%d size=%d vArray = 0x%llx\n", 
		addr, row, size, vArray);

	L1Entry* entry = getEntry(row);
	assert(isDirty(entry));

	// copy timestamps to the target TimeTable
	// if 2nd cache is used, different scheme required		
	int i;
	Time* tArray = getTimeAddr(row, 0);
	//MShadowSetFromCache(addr, size, vArray, tArray);
	for (i=size-1; i>=0; i--) {
		if (matchVersion(addr, i, vArray[i])) {
			// write to MShadow
			MShadowSetTimeEvict(addr, i, vArray[i], tArray[i]);
		}
	}
	
	return entry;
}

void MShadowFetchLine(L1Entry* entry, Addr addr, Index size, Version* vArray) {
	int index;
	int row = getLineIndex(addr);

	// copy version
	Version* versionAddr = (Version*) getVersionAddr(row, 0);
	memcpy(versionAddr, vArray, sizeof(Version) * size);

	// bring MShadow data
	Time* destAddr = getTimeAddr(row, 0);
	for (index=0; index<size; index++) {
		*(destAddr + index) = MShadowGetTime(addr, index, vArray[index]);
	}

	entry->tag = getTag(addr);
	setValid(entry);
	resetDirty(entry);
}

#ifdef BYPASS_CACHE

Time tempArray[1000];
Time* MShadowGet(Addr addr, Index size, Version* vArray) {
	Index i;
	for (i=0; i<size; i++)
		tempArray[i] = MShadowGetTime(addr, i, vArray[i]);
	return tempArray;	
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray) {
	MShadowSetFromCache(addr, size, vArray, tArray);
}

#else

Time* MShadowGet(Addr addr, Index size, Version* vArray) {
	int row = getLineIndex(addr);
	int i;

	MSG(0, "MShadowGet 0x%llx, size %d vArray = 0x%llx \n", addr, size, vArray);
	eventRead();

	L1Entry* entry = getEntry(row);
	Time* destAddr = getTimeAddr(row, 0);
	if (isHit(entry, addr)) {
		eventReadHit();
		MSG(0, "\t cache hit at 0x%llx \n", destAddr);
		MSG(0, "\t value0 %d value1 %d \n", destAddr[0], destAddr[1]);
		
	} else {
		// Unfortunately, this access results in a miss
		// 1. evict a line	
		if (isDirty(entry)) {
			MSG(0, "\t eviction required \n", destAddr);
			eventReadEvict();
			MShadowL1Evict(addr, row, size, vArray);
		}

		// 2. read line from MShadow to the evicted line
		MSG(0, "\t write values to cache \n", destAddr);
		MShadowFetchLine(entry, addr, size, vArray);
	}

	// check versions and if outdated, set to 0
	MSG(0, "\t checking versions \n", destAddr);
	Version* vAddr = (Version*) getVersionAddr(row, 0);

	for (i=size-1; i>=0; i--) {
		if (vAddr[i] ==  vArray[i]) {
			// no need to check next iterations
			break;

		} else {
			// update version number 	
			// and set timestamp to zero
			vAddr[i] = vArray[i];
			destAddr[i] = 0ULL;
		}
	}

	return destAddr;
}

void MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray) {
	MSG(0, "MShadowSet 0x%llx, size %d vArray = 0x%llx tArray = 0x%llx\n", addr, size, vArray, tArray);
	int row = getLineIndex(addr);
	eventWrite();

	L1Entry* entry = getEntry(row);

	assert(row < NUM_LINE);

	if (isHit(entry, addr)) {
		eventWriteHit();

	} else {
		if (isDirty(entry)) {
			MSG(0, "\t eviction required\n", row, 0);
			eventWriteEvict();
			MShadowL1Evict(addr, row, size, vArray);
		}
	} 		


	// copy Timestamps
	Time* destAddr = getTimeAddr(row, 0);
	memcpy(destAddr, tArray, sizeof(Time) * size);

	// copy Versions
	Version* versionAddr = (Version*) getVersionAddr(row, 0);
	int i;
	for (i=size-1; i>=0; i--) {
		if (versionAddr[i] == vArray[i])
			break;
		else
			versionAddr[i] = vArray[i];
	}
	//memcpy(versionAddr, vArray, sizeof(Version) * size);

	setDirty(entry);
}
#endif

#endif

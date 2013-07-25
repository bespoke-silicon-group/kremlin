#ifndef _MSHADOW_STAT
#define _MSHADOW_STAT

#include "ktypes.h"

void MShadowStatPrint();

/*
 * Allocation Stat Structure
 */


typedef struct _AllocStat {
	UInt64 nAlloc;
	UInt64 nDealloc;
	UInt64 nConvertIn;
	UInt64 nConvertOut;
	UInt64 nActive;
	UInt64 nActiveMax;
	
} AStat;

static inline void AStatAlloc(AStat* stat) {
	stat->nAlloc++;
	stat->nActive++;
	if (stat->nActiveMax < stat->nActive)
		stat->nActiveMax = stat->nActive;
}

static inline void AStatDealloc(AStat* stat) {
	stat->nDealloc++;
	stat->nActive--;
}

static inline void AStatConvertIn(AStat* stat) {
	stat->nConvertIn++;
	stat->nActive++;

	if (stat->nActiveMax < stat->nActive)
		stat->nActiveMax = stat->nActive;
}

static inline void AStatConvertOut(AStat* stat) {
	stat->nConvertOut++;
	stat->nActive--;
}


/*
 * Level Specific Stat
 */

typedef struct _LevelStat {
	AStat  segTable;
	AStat  tTable[2];
	UInt64 nTimeTableRealoc;
	UInt64 nLevelWrite;
} LStat;


/*
 * MemStat
 */


typedef struct _MemStat {
	LStat levels[128];	

	AStat segTable;
	AStat tTable[2];

	AStat lTable;

	UInt64 nGC;

	// tracking overhead of timetables (in bytes) with compression
	UInt64 timeTableOverhead;
	UInt64 timeTableOverheadMax;

} MemStat;

extern MemStat _stat;

static inline void eventLevelTableAlloc() {
	//_stat.nLevelTableAlloc++;
	AStatAlloc(&_stat.lTable);
}

static inline void eventTimeTableNewAlloc(int level, int type) {
	//_stat.levels[level].nTimeTableNewAlloc++;
	AStatAlloc(&_stat.levels[level].tTable[type]);
}


static inline void eventTimeTableConvert(int level) {
	//_stat.levels[level].nTimeTableConvert++;
	AStatConvertOut(&_stat.levels[level].tTable[0]);
	AStatConvertIn(&_stat.levels[level].tTable[1]);
}

static inline void eventLevelWrite(int level) {
	_stat.levels[level].nLevelWrite++;
}

static inline void eventTimeTableConvertTo32() {
	AStatConvertOut(&_stat.tTable[0]);
	AStatConvertIn(&_stat.tTable[1]);
}

static inline void increaseTimeTableMemSize(int size) {
	_stat.timeTableOverhead += size;
	if(_stat.timeTableOverhead > _stat.timeTableOverheadMax)
		_stat.timeTableOverheadMax = _stat.timeTableOverhead;
}

static inline void decreaseTimeTableMemSize(int size) {
	_stat.timeTableOverhead -= size;
}

static inline void eventCompression(int gain) {
	decreaseTimeTableMemSize(gain);
}

static inline void eventTimeTableAlloc(int sizeType, int size) {
	AStatAlloc(&_stat.tTable[sizeType]);
	increaseTimeTableMemSize(size);
}

static inline void eventTimeTableFree(int type, int size) {
	AStatDealloc(&_stat.tTable[type]);
	decreaseTimeTableMemSize(size);
}

static inline void eventSegTableAlloc() {
	AStatAlloc(&_stat.segTable);
}

static inline void eventSegTableFree() {
	AStatDealloc(&_stat.segTable);
}

static inline void eventGC() {
	_stat.nGC++;
}


static inline UInt64 getActiveTimeTableSize() {
	return _stat.tTable[0].nActive + _stat.tTable[1].nActive;
}

/*
 * CacheStat
 */

typedef struct _L1Stat {
	UInt64 nRead;
	UInt64 nReadHit;
	UInt64 nReadEvict;
	UInt64 nWrite;
	UInt64 nWriteHit;
	UInt64 nWriteEvict;

	UInt64 nEvictLevel[128];
	UInt64 nEvictTotal;
	UInt64 nCacheEvictLevelTotal;
	UInt64 nCacheEvictLevelEffective;
	UInt64 nCacheEvict;


} L1Stat;

extern L1Stat _cacheStat;


static inline void eventRead() {
	_cacheStat.nRead++;
}

static inline void eventReadHit() {
	_cacheStat.nReadHit++;
}

static inline void eventReadEvict() {
	_cacheStat.nReadEvict++;
}

static inline void eventWrite() {
	_cacheStat.nWrite++;
}

static inline void eventWriteHit() {
	_cacheStat.nWriteHit++;
}

static inline void eventWriteEvict() {
	_cacheStat.nWriteEvict++;
}

static inline void eventCacheEvict(int total, int effective) {
	_cacheStat.nCacheEvictLevelTotal += total;
	_cacheStat.nCacheEvictLevelEffective += effective;
	_cacheStat.nCacheEvict++;
}

static inline void eventEvict(int level) {
	_cacheStat.nEvictLevel[level]++;
	_cacheStat.nEvictTotal++;
}

#endif

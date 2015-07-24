#ifndef _MSHADOW_STAT
#define _MSHADOW_STAT

#include "ktypes.h"
#include "TimeTable.hpp" // for TimeTable::TableType (TODO: avoid this)

void MShadowStatPrint();

/*
 * Allocation Stat Structure
 */


typedef struct _AllocStat {
	uint64_t nAlloc;
	uint64_t nDealloc;
	uint64_t nConvertIn;
	uint64_t nConvertOut;
	uint64_t nActive;
	uint64_t nActiveMax;
	
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
	uint64_t nTimeTableRealoc;
	uint64_t nLevelWrite;
} LStat;


/*
 * MemStat
 */


typedef struct _MemStat {
	LStat levels[128];	

	AStat segTable;
	AStat tTable[2];

	AStat lTable;

	uint64_t nGC;

	// tracking overhead of timetables (in bytes) with compression
	uint64_t timeTableOverhead;
	uint64_t timeTableOverheadMax;

} MemStat;

extern MemStat _stat;

static inline void eventLevelTableAlloc() {
	//_stat.nLevelTableAlloc++;
	AStatAlloc(&_stat.lTable);
}

static inline void eventTimeTableNewAlloc(int level, TimeTable::TableType type) {
	//_stat.levels[level].nTimeTableNewAlloc++;
	AStatAlloc(&_stat.levels[level].tTable[(int)type]);
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

static inline void eventTimeTableAlloc(TimeTable::TableType sizeType, int size) {
	AStatAlloc(&_stat.tTable[(int)sizeType]);
	increaseTimeTableMemSize(size);
}

static inline void eventTimeTableFree(TimeTable::TableType type, int size) {
	AStatDealloc(&_stat.tTable[(int)type]);
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


static inline uint64_t getActiveTimeTableSize() {
	return _stat.tTable[0].nActive + _stat.tTable[1].nActive;
}

/*
 * CacheStat
 */

typedef struct _L1Stat {
	uint64_t nRead;
	uint64_t nReadHit;
	uint64_t nReadEvict;
	uint64_t nWrite;
	uint64_t nWriteHit;
	uint64_t nWriteEvict;

	uint64_t nEvictLevel[128];
	uint64_t nEvictTotal;
	uint64_t nCacheEvictLevelTotal;
	uint64_t nCacheEvictLevelEffective;
	uint64_t nCacheEvict;


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

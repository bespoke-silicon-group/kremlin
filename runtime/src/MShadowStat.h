#ifndef _MSHADOW_STAT
#define _MSHADOW_STAT

#include "ktypes.h"

/*
 * MemStat
 */
typedef struct _MemStat {

	UInt64 nSegTableAllocated;
	UInt64 nSegTableActive;
	UInt64 nSegTableActiveMax;

	UInt64 nTimeTableAllocated[2];
	UInt64 nTimeTableFreed[2];
	UInt64 nVersionTableAllocated[2];
	UInt64 nVersionTableFreed[2];
	UInt64 nTimeTableConvert32;
	UInt64 nTimeTableActive;
	UInt64 nTimeTableActiveMax;
	UInt64 nSegTableNewAlloc[100];
	UInt64 nLTableAlloc;
	UInt64 nTimeTableNewAlloc[100];
	UInt64 nTimeTableConvert[100];
	UInt64 nTimeTableRealloc[100];
	UInt64 nTimeTableReallocTotal;
	UInt64 nLevelWrite[100];
	UInt64 nEvict[100];
	UInt64 nEvictTotal;
	UInt64 nCacheEvictLevelTotal;
	UInt64 nCacheEvictLevelEffective;
	UInt64 nCacheEvict;
	UInt64 nGC;
	UInt64 nLTableAccess;

	// tracking overhead of timetables (in bytes) with compression
	UInt64 timeTableOverhead;
	UInt64 timeTableOverheadMax;

#if COMPRESSION_POLICY == 1
	UInt64 nActiveTableHits;
	UInt64 nActiveTableMisses;
#endif
} MemStat;


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
} L1Stat;

extern MemStat _stat;
extern L1Stat _cacheStat;

static inline void eventLTableAlloc() {
	_stat.nLTableAlloc++;
}

static inline void eventCacheEvict(int total, int effective) {
	_stat.nCacheEvictLevelTotal += total;
	_stat.nCacheEvictLevelEffective += effective;
	_stat.nCacheEvict++;
}

static inline void eventTimeTableNewAlloc(int level) {
	_stat.nTimeTableNewAlloc[level]++;
}

static inline void eventTimeTableConvert(int level) {
	_stat.nTimeTableConvert[level]++;
}

static inline void eventLevelWrite(int level) {
	_stat.nLevelWrite[level]++;
}

static inline void eventTimeTableConvertTo32() {
	_stat.nTimeTableConvert32++;
}


static inline void eventTimeTableRealloc(int level) {
	_stat.nTimeTableRealloc[level]++;
	_stat.nTimeTableReallocTotal++;
}

static inline void eventEvict(int level) {
	_stat.nEvict[level]++;
	_stat.nEvictTotal++;
}
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


static inline void TimeTableUpdateOverhead(int size) {
	_stat.timeTableOverhead += size;
	if(_stat.timeTableOverhead > _stat.timeTableOverheadMax)
		_stat.timeTableOverheadMax = _stat.timeTableOverhead;
}

static inline void eventTimeTableAlloc(int sizeType) {
	_stat.nTimeTableAllocated[sizeType]++;
	_stat.nTimeTableActive++;
	if (_stat.nTimeTableActive > _stat.nTimeTableActiveMax)
		_stat.nTimeTableActiveMax++;
}

static inline void eventTimeTableFree(int type) {
	_stat.nTimeTableActive--;
	_stat.nTimeTableFreed[type]++;
}

static inline void eventSegTableAlloc() {
	_stat.nSegTableAllocated++;
	_stat.nSegTableActive++;
	if (_stat.nSegTableActive > _stat.nSegTableActiveMax)
		_stat.nSegTableActiveMax++;
}

static inline void eventSegTableFree() {
	_stat.nSegTableActive--;
}

static inline void eventGC() {
	_stat.nGC++;
}

static inline void eventLTableAccess() {
	_stat.nLTableAccess++;
}

static inline UInt64 getActiveTimeTableSize() {
	return _stat.nTimeTableActiveMax;
}
#endif

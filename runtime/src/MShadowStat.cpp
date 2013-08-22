#include "debug.h"
#include "kremlin.h"
#include "config.h"

#include "MemorySegment.hpp"
#include "LevelTable.hpp"
#include "MShadowStat.h"
#include "MShadowSkadu.h"

MemStat _stat;
L1Stat _cacheStat;



/*
 * Cache size when a specific # of levels are used
 */
static int cacheMB;
double getCacheSize(int level) {
	int cacheSize = kremlin_config.getShadowMemCacheSizeInMB();
	return (double)(cacheSize* 4.0 + level * cacheSize * 2);
}

static double getSizeMB(UInt64 nUnit, UInt64 size) {
	return (nUnit * size) / (1024.0 * 1024.0);	
}

void printCacheStat() {
	MSG(0, "\nShadow Memory Cache Stat\n");	
	MSG(0, "\tread  all / hit / evict = %llu / %llu / %llu\n", 
		_cacheStat.nRead, _cacheStat.nReadHit, _cacheStat.nReadEvict);
	MSG(0, "\twrite all / hit / evict = %llu / %llu / %llu\n", 
		_cacheStat.nWrite, _cacheStat.nWriteHit, _cacheStat.nWriteEvict);
	double hitRead = _cacheStat.nReadHit * 100.0 / _cacheStat.nRead;
	double hitWrite = _cacheStat.nWriteHit * 100.0 / _cacheStat.nWrite;
	double hit = (_cacheStat.nReadHit + _cacheStat.nWriteHit) * 100.0 / (_cacheStat.nRead + _cacheStat.nWrite);
	MSG(0, "\tCache hit (read / write / overall) = %.2f / %.2f / %.2f\n", 
		hitRead, hitWrite, hit);
	MSG(0, "\tEvict (total / levelAvg / levelEffective) = %llu / %.2f / %.2f\n\n", 
		_cacheStat.nCacheEvict, 
		(double)_cacheStat.nCacheEvictLevelTotal / _cacheStat.nCacheEvict, 
		(double)_cacheStat.nCacheEvictLevelEffective / _cacheStat.nCacheEvict);

	MSG(0, "\tnGC = %llu\n", _stat.nGC);
}


void printMemReqStat() {
	//fprintf(stderr, "Overall allocated = %d, converted = %d, realloc = %d\n", 
	//	totalAlloc, totalConvert, totalRealloc);
	double segSize = getSizeMB(_stat.segTable.nActiveMax, sizeof(MemorySegment));
	double lTableSize = getSizeMB(_stat.lTable.nActiveMax, sizeof(LevelTable));

	UInt64 nTable0 = _stat.tTable[0].nActiveMax;
	UInt64 nTable1 = _stat.tTable[1].nActiveMax;
	int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TimeTable::TIMETABLE_SIZE / 2);
	int sizeTable32 = sizeof(TimeTable) + sizeof(Time) * TimeTable::TIMETABLE_SIZE;
	//double tTableSize1 = getSizeMB(nTable1, sizeTable32);
	//double tTableSize = tTableSize0 + tTableSize1;

	UInt64 sizeUncompressed = sizeTable64 + sizeTable32;
	double tTableSize = getSizeMB(sizeUncompressed, 1);
	double tTableSizeWithCompression = getSizeMB(_stat.timeTableOverheadMax, 1);

	double cacheSize = getCacheSize(getMaxActiveLevel());
	double totalSize = segSize + lTableSize + tTableSize + cacheSize;
	double totalSizeComp = segSize + lTableSize + cacheSize + tTableSizeWithCompression;
	
	UInt64 compressedNoBuffer = _stat.timeTableOverheadMax - kremlin_config.getNumCompressionBufferEntries() * sizeTable64;
	UInt64 noCompressedNoBuffer = sizeUncompressed - kremlin_config.getNumCompressionBufferEntries() * sizeTable64;
	double compressionRatio = (double)noCompressedNoBuffer / compressedNoBuffer;

	//minTotal += getCacheSize(2);
	//fprintf(stderr, "%ld, %ld, %ld\n", _stat.timeTableOverhead, sizeUncompressed, _stat.timeTableOverhead - sizeUncompressed);

	MSG(0, "\nRequired Memory Analysis\n");
	MSG(0, "\tShadowMemory (MemorySegment / LevTable/ TTable / TTableCompressed) = %.2f / %.2f/ %.2f / %.2f \n",
		segSize, lTableSize, tTableSize, tTableSizeWithCompression);
	MSG(0, "\tReqMemSize (Total / Cache / Uncompressed Shadow / Compressed Shadow) = %.2f / %.2f / %.2f / %.2f\n",
		totalSize, cacheSize, segSize + tTableSize, segSize + tTableSizeWithCompression);  
	MSG(0, "\tTagTable (Uncompressed / Compressed / Ratio / Comp Ratio) = %.2f / %.2f / %.2f / %.2f\n",
		tTableSize, tTableSizeWithCompression, tTableSize / tTableSizeWithCompression, compressionRatio);
	MSG(0, "\tTotal (Uncompressed / Compressed / Ratio) = %.2f / %.2f / %.2f\n",
		totalSize, totalSizeComp, totalSize / totalSizeComp);
}


static void printMemStatAllocation() {
	MSG(0, "\nShadow Memory Allocation Stats\n");
	MSG(0, "\tnMemorySegment: Alloc / Active / ActiveMax = %llu / %llu / %llu\n",
		 _stat.segTable.nAlloc, _stat.segTable.nActive, _stat.segTable.nActiveMax);

	MSG(0, "\tnTimeTable(type %d): Alloc / Freed / ActiveMax = %llu / %llu / %llu / %llu\n",
		 0, _stat.tTable[0].nAlloc, _stat.tTable[0].nDealloc, _stat.tTable[0].nConvertOut, _stat.tTable[0].nActiveMax);
	MSG(0, "\tnTimeTable(type %d): Alloc / Freed / ActiveMax = %llu / %llu / %llu / %llu\n",
		 1, _stat.tTable[1].nAlloc, _stat.tTable[1].nDealloc, _stat.tTable[1].nConvertIn, _stat.tTable[1].nActiveMax);
}


void printLevelStat() {
	int i;
	int totalAlloc = 0;
	int totalConvert = 0;
	int totalRealloc = 0;
	double minTotal = 0;

	MSG(0, "\nLevel Specific Statistics\n");

#if 0
	for (i=0; i<=getMaxActiveLevel(); i++) {
		totalAlloc += _stat.nTimeTableNewAlloc[i];
		totalConvert += _stat.nTimeTableConvert[i];
		totalRealloc += _stat.nTimeTableRealloc[i];

		int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TimeTable::TIMETABLE_SIZE / 2);
		double sizeMemorySegment = getSizeMB(_stat.nSegTableNewAlloc[i], sizeof(MemorySegment));
		double sizeTimeTable = getSizeMB(_stat.nTimeTableNewAlloc[i], sizeTable64);
		double sizeVersionTable = getSizeMB(_stat.nTimeTableConvert[i], sizeof(TimeTable));
		double sizeLevel = sizeMemorySegment + sizeTimeTable + sizeVersionTable;
		double reallocPercent = (double)_stat.nTimeTableRealloc[i] * 100.0 / (double)_stat.nEvict[i];

		if (i < 2)
			minTotal += sizeLevel;
		
		fprintf(stderr, "\tLevel [%2d] Wr Cnt = %lld, TTable=%.2f, VTable=%.2f Sum=%.2f MB\n", 
			i, _stat.nLevelWrite[i], sizeTimeTable, sizeVersionTable, sizeLevel);
		fprintf(stderr, "\t\tReallocPercent=%.2f, Evict=%lld, Realloc=%lld\n",
			reallocPercent, _stat.nEvict[i], _stat.nTimeTableRealloc[i]);
			
	}
#endif
}

void MShadowStatPrint() {
	printMemStatAllocation();
	//printLevelStat();
	printCacheStat();
	printMemReqStat();
}


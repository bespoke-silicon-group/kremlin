#include "MShadowStat.h"
#include "MShadowSkadu.h"
#include "config.h"

MemStat _stat;
L1Stat _cacheStat;



/*
 * Cache size when a specific # of levels are used
 */
static int cacheMB;
double getCacheSize(int level) {
	int cacheSize = KConfigGetCacheSize();
	return (double)(cacheSize* 4.0 + level * cacheSize * 2);
}

static double getSizeMB(UInt64 nUnit, UInt64 size) {
	return (nUnit * size) / (1024.0 * 1024.0);	
}

void printCacheStat() {
	fprintf(stderr, "\nShadow Memory Cache Stat\n");	
	fprintf(stderr, "\tread  all / hit / evict = %ld / %ld / %ld\n", 
		_cacheStat.nRead, _cacheStat.nReadHit, _cacheStat.nReadEvict);
	fprintf(stderr, "\twrite all / hit / evict = %ld / %ld / %ld\n", 
		_cacheStat.nWrite, _cacheStat.nWriteHit, _cacheStat.nWriteEvict);
	double hitRead = _cacheStat.nReadHit * 100.0 / _cacheStat.nRead;
	double hitWrite = _cacheStat.nWriteHit * 100.0 / _cacheStat.nWrite;
	double hit = (_cacheStat.nReadHit + _cacheStat.nWriteHit) * 100.0 / (_cacheStat.nRead + _cacheStat.nWrite);
	fprintf(stderr, "\tCache hit (read / write / overall) = %.2f / %.2f / %.2f\n", 
		hitRead, hitWrite, hit);
	fprintf(stderr, "\tEvict (total / levelAvg / levelEffective) = %ld / %.2f / %.2f\n\n", 
		_cacheStat.nCacheEvict, 
		(double)_cacheStat.nCacheEvictLevelTotal / _cacheStat.nCacheEvict, 
		(double)_cacheStat.nCacheEvictLevelEffective / _cacheStat.nCacheEvict);

	fprintf(stderr, "\tnGC = %ld\n", _stat.nGC);
}


void printMemReqStat() {
	//fprintf(stderr, "Overall allocated = %d, converted = %d, realloc = %d\n", 
	//	totalAlloc, totalConvert, totalRealloc);
	double segSize = getSizeMB(_stat.segTable.nActiveMax, sizeof(SegTable));
	double lTableSize = getSizeMB(_stat.lTable.nActiveMax, sizeof(LTable));

	UInt64 nTable0 = _stat.tTable[0].nActiveMax;
	UInt64 nTable1 = _stat.tTable[1].nActiveMax;
	int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TIMETABLE_SIZE / 2);
	int sizeTable32 = sizeof(TimeTable) + sizeof(Time) * TIMETABLE_SIZE;
	//double tTableSize1 = getSizeMB(nTable1, sizeTable32);
	//double tTableSize = tTableSize0 + tTableSize1;

	UInt64 sizeUncompressed = sizeTable64 + sizeTable32;
	double tTableSize = getSizeMB(sizeUncompressed, 1);
	double tTableSizeWithCompression = getSizeMB(_stat.timeTableOverheadMax, 1);

	double cacheSize = getCacheSize(getMaxActiveLevel());
	double totalSize = segSize + lTableSize + tTableSize + cacheSize;
	double totalSizeComp = segSize + lTableSize + cacheSize + tTableSizeWithCompression;
	
	UInt64 compressedNoBuffer = _stat.timeTableOverheadMax - KConfigGetCBufferSize() * sizeTable64;
	UInt64 noCompressedNoBuffer = sizeUncompressed - KConfigGetCBufferSize() * sizeTable64;
	double compressionRatio = (double)noCompressedNoBuffer / compressedNoBuffer;

	//minTotal += getCacheSize(2);
	//fprintf(stderr, "%ld, %ld, %ld\n", _stat.timeTableOverhead, sizeUncompressed, _stat.timeTableOverhead - sizeUncompressed);

	fprintf(stderr, "\nRequired Memory Analysis\n");
	fprintf(stderr, "\tShadowMemory (SegTable / LevTable/ TTable / TTableCompressed) = %.2f / %.2f/ %.2f / %.2f \n",
		segSize, lTableSize, tTableSize, tTableSizeWithCompression);
	fprintf(stderr, "\tReqMemSize (Total / Cache / Uncompressed Shadow / Compressed Shadow) = %.2f / %.2f / %.2f / %.2f\n",
		totalSize, cacheSize, segSize + tTableSize, segSize + tTableSizeWithCompression);  
	fprintf(stderr, "\tTagTable (Uncompressed / Compressed / Ratio / Comp Ratio) = %.2f / %.2f / %.2f / %.2f\n",
		tTableSize, tTableSizeWithCompression, tTableSize / tTableSizeWithCompression, compressionRatio);
	fprintf(stderr, "\tTotal (Uncompressed / Compressed / Ratio) = %.2f / %.2f / %.2f\n",
		totalSize, totalSizeComp, totalSize / totalSizeComp);
}


static void printMemStatAllocation() {
	fprintf(stderr, "\nShadow Memory Allocation Stats\n");
	fprintf(stderr, "\tnSegTable: Alloc / Active / ActiveMax = %ld / %ld / %ld\n",
		 _stat.segTable.nAlloc, _stat.segTable.nActive, _stat.segTable.nActiveMax);

	fprintf(stderr, "\tnTimeTable(type %d): Alloc / Freed / ActiveMax = %ld / %ld / %ld / %ld\n",
		 0, _stat.tTable[0].nAlloc, _stat.tTable[0].nDealloc, _stat.tTable[0].nConvertOut, _stat.tTable[0].nActiveMax);
	fprintf(stderr, "\tnTimeTable(type %d): Alloc / Freed / ActiveMax = %ld / %ld / %ld / %ld\n",
		 1, _stat.tTable[1].nAlloc, _stat.tTable[1].nDealloc, _stat.tTable[1].nConvertIn, _stat.tTable[1].nActiveMax);
}


void printLevelStat() {
	int i;
	int totalAlloc = 0;
	int totalConvert = 0;
	int totalRealloc = 0;
	double minTotal = 0;

	fprintf(stderr, "\nLevel Specific Statistics\n");

#if 0
	for (i=0; i<=getMaxActiveLevel(); i++) {
		totalAlloc += _stat.nTimeTableNewAlloc[i];
		totalConvert += _stat.nTimeTableConvert[i];
		totalRealloc += _stat.nTimeTableRealloc[i];

		int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TIMETABLE_SIZE / 2);
		double sizeSegTable = getSizeMB(_stat.nSegTableNewAlloc[i], sizeof(SegTable));
		double sizeTimeTable = getSizeMB(_stat.nTimeTableNewAlloc[i], sizeTable64);
		double sizeVersionTable = getSizeMB(_stat.nTimeTableConvert[i], sizeof(TimeTable));
		double sizeLevel = sizeSegTable + sizeTimeTable + sizeVersionTable;
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

void printMemStat() {
	printMemStatAllocation();
	printLevelStat();
	printCacheStat();
	printMemReqStat();
}


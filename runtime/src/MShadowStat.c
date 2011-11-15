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
		_stat.nCacheEvict, 
		(double)_stat.nCacheEvictLevelTotal / _stat.nCacheEvict, 
		(double)_stat.nCacheEvictLevelEffective / _stat.nCacheEvict);

	fprintf(stderr, "\tnGC = %ld\n", _stat.nGC);
}


void printMemReqStat() {
	//fprintf(stderr, "Overall allocated = %d, converted = %d, realloc = %d\n", 
	//	totalAlloc, totalConvert, totalRealloc);
	double segSize = getSizeMB(_stat.nSegTableActiveMax, sizeof(SegTable));
	double lTableSize = getSizeMB(_stat.nLTableAlloc, sizeof(LTable));

	// XXX FIXME: nTable0 is unused and nTable1 is probably used incorrectly!
	UInt64 nTable0 = _stat.nTimeTableAllocated[0] - _stat.nTimeTableFreed[0];
	int sizeTable64 = sizeof(TimeTable) + sizeof(Time) * (TIMETABLE_SIZE / 2);
	UInt64 nTable1 = _stat.nTimeTableAllocated[1] - _stat.nTimeTableFreed[1];
	int sizeTable32 = sizeof(TimeTable) + sizeof(Time) * TIMETABLE_SIZE;
	//double tTableSize1 = getSizeMB(nTable1, sizeTable32);
	//double tTableSize = tTableSize0 + tTableSize1;

	UInt64 sizeUncompressed = _stat.nTimeTableActiveMax * sizeof(Time) * TIMETABLE_SIZE / 2;
	double tTableSize = getSizeMB(sizeUncompressed, 1);
	double tTableSizeWithCompression = getSizeMB(_stat.timeTableOverheadMax, 1);


	int sizeVersion64 = sizeof(Version) * (TIMETABLE_SIZE / 2);
	int sizeVersion32 = sizeof(Version) * (TIMETABLE_SIZE);
	UInt64 nVTable0 = _stat.nVersionTableAllocated[0] - _stat.nVersionTableFreed[0];
	UInt64 nVTable1 = _stat.nVersionTableAllocated[1] - _stat.nVersionTableFreed[1];
	double vTableSize0 = getSizeMB(nVTable0, sizeVersion64);
	double vTableSize1 = getSizeMB(nVTable1, sizeVersion32);
	double vTableSize = vTableSize0 + vTableSize1;


	double cacheSize = getCacheSize(getMaxActiveLevel());
	double totalSize = segSize + lTableSize + tTableSize + cacheSize;
	double totalSizeComp = segSize + lTableSize + cacheSize + tTableSizeWithCompression;
	
	UInt64 compressedNoBuffer = _stat.timeTableOverheadMax - KConfigGetCBufferSize() * sizeTable64;
	UInt64 noCompressedNoBuffer = sizeUncompressed - KConfigGetCBufferSize() * sizeTable64;
	double compressionRatio = (double)noCompressedNoBuffer / compressedNoBuffer;

	//minTotal += getCacheSize(2);
	fprintf(stderr, "%ld, %ld, %ld\n", _stat.timeTableOverhead, sizeUncompressed, _stat.timeTableOverhead - sizeUncompressed);
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
		 _stat.nSegTableAllocated, _stat.nSegTableActive, _stat.nSegTableActiveMax);
	fprintf(stderr, "\tnTimeTable(type %d): Alloc / Freed / ActiveMax = %ld, %ld / %ld, %ld / %ld\n",
		 0, _stat.nTimeTableAllocated[0], _stat.nTimeTableAllocated[1],
		 _stat.nTimeTableFreed[0], _stat.nTimeTableFreed[1],
		 _stat.nTimeTableActiveMax);
	fprintf(stderr, "\tnTimeTable(type %d): Alloc / Freed / ActiveMax= %ld, %ld / %ld, %ld / %ld\n",
		 0, _stat.nTimeTableAllocated[0], _stat.nTimeTableAllocated[1],
		 _stat.nTimeTableFreed[0], _stat.nTimeTableFreed[1], _stat.nTimeTableActiveMax);
	fprintf(stderr, "\tbTimeTable Convert: %ld\n", _stat.nTimeTableConvert32);
	fprintf(stderr, "\tnVersionTable: Alloc = %l / %l\n", 
		_stat.nVersionTableAllocated[0], _stat.nVersionTableAllocated[1]);
	fprintf(stderr, "\tnTotal Evict: %l, Realloc: %l\n", 
		_stat.nEvictTotal, _stat.nTimeTableReallocTotal);
	fprintf(stderr, "\tRealloc / Evict Percentage: %.2f %%\n", 
		(double)_stat.nTimeTableReallocTotal * 100.0 / _stat.nEvictTotal);
	UInt64 nAllocated = _stat.nTimeTableAllocated[0] + _stat.nTimeTableAllocated[1];
	fprintf(stderr, "\tNewAlloc / Evict Percentage: %.2f %%\n", 
		(double)nAllocated * 100.0 / _stat.nEvictTotal);
}


void printLevelStat() {
	int i;
	int totalAlloc = 0;
	int totalConvert = 0;
	int totalRealloc = 0;
	double minTotal = 0;

	fprintf(stderr, "\nLevel Specific Statistics\n");

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
}

void printMemStat() {
	printMemStatAllocation();
	printLevelStat();
	printCacheStat();
	printMemReqStat();
}


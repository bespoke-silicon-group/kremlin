package pprof;

public class CacheManager {
	CRegionManager manager;
	CacheLevel levels[];
	CacheStat stat;
	class CacheLevel {
		CacheLevel(int time, int limit) {
			this.serviceTime = time;
			this.scaleLimit = limit;
		}
		int serviceTime;
		int scaleLimit;
	}	
	
	public CacheManager(CRegionManager manager, String cacheFile) {
		this.manager = manager;
		this.levels = new CacheLevel[2];
		this.levels[0] = new CacheLevel(10, 64);
		this.levels[1] = new CacheLevel(200, 8);
		this.stat = new CacheStat(cacheFile);
	}
	
	public CRegionManager getCRegionManager() {
		return this.manager;
	}
	
	
	double getReadMissTime(CRegion region, int level, int core) {
		double loadCnt = region.avgLoadCnt;
		double readMissCnt = loadCnt * stat.getReadMissRate(core, level);
		return readMissCnt * levels[level].serviceTime;		
	}
	
	double getWriteMissTime(CRegion region, int level, int core) {
		double storeCnt = region.avgStoreCnt;
		double writeMissCnt = storeCnt * stat.getWriteMissRate(core, level);
		return writeMissCnt * levels[level].serviceTime;
	}
		
	public double getCacheServiceTime(CRegion region, int core) {
		int maxLevel = levels.length;
		double sum = 0.0;
		
		for (int level=0; level<maxLevel; level++) {
			double readTime = getReadMissTime(region, level, core);
			double writeTime = getWriteMissTime(region, level, core);
			int divider = (core > levels[level].scaleLimit) ? levels[level].scaleLimit : core;
			sum += (readTime + writeTime) / divider;			
		}	
		return sum;		
	}
}

package planner;

import pprof.*;

public class CacheAwarePlanner extends CDPPlanner {
	CacheManager cacheManager;
	public CacheAwarePlanner(CacheManager manager, Target target) {		
		super(manager.getCRegionManager(), target);
		this.cacheManager = manager;
	}
	
	protected double getParallelTime(CRegion region) {
		double spSpeedup = (this.maxCore < region.getSelfP()) ? maxCore : region.getSelfP();
		double parallelTime = region.getAvgWork() / spSpeedup + overhead;
		double cacheServiceTime = cacheManager.getCacheServiceTime(region, (int)spSpeedup);
		return parallelTime + cacheServiceTime;
	}
	
	@Override
	protected double getSerialTime(CRegion region) {
		double cacheServiceTime = cacheManager.getCacheServiceTime(region, 1);
		return region.getAvgWork() + cacheServiceTime;
	}
}
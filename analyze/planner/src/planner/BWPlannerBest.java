package planner;

import pprof.CRegion;
import pprof.CRegionManager;

public class BWPlannerBest extends CDPPlanner {
	public BWPlannerBest(CRegionManager analyzer, Target target) {
		super(analyzer, target);
	}
	// TCC Processor: E550, 8MB Cache, 2.4GHz, 5.86GT/s
	// STREAMS Bench says: 25000 MB / s
	protected double getParallelTime(CRegion region) {
		double spSpeedup = (this.maxCore < region.getSelfParallelism()) ? maxCore : region.getSelfParallelism();
		double parallelTime = region.getAvgWork() / spSpeedup + overhead;
		
		long cacheSize = target.getCacheMB();
		long clockRateMhz = target.getClockMHz();
		long bwMB = target.getBandwidthMB();
		long dataByte = (long)(region.getAvgReadCnt() + region.getAvgWriteCnt()) * 4 - cacheSize;
		double cycle = (double)clockRateMhz * dataByte / (double)bwMB; 
		double parallelBwTime = cycle;
		
		return (parallelBwTime > parallelTime) ? parallelBwTime : parallelTime;
	}	
}

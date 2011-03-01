package planner;

import pprof.CRegion;
import pprof.CRegionManager;

public class BWPlannerWorst extends CDPPlanner {
	public BWPlannerWorst(CRegionManager analyzer, Target target) {
		super(analyzer, target);
	}
	// TCC Processor: E550, 8MB Cache, 2.4GHz, 5.86GT/s
	// STREAMS Bench says: 25000 MB / s
	protected double getParallelTime(CRegion region) {
		double spSpeedup = (this.maxCore < region.getSelfParallelism()) ? maxCore : region.getSelfParallelism();
		double parallelTime = region.getAvgWork() / spSpeedup + overhead;
		
		long dataByte = (long)(region.getAvgReadCnt() + region.getAvgWriteCnt()) * 4;
		double cycle = (double)2400.0 * dataByte / (double)10000.0; 
		double parallelBwTime = cycle;
		
		return (parallelBwTime > parallelTime) ? parallelBwTime : parallelTime;
	}	
}

package planner;

import pprof.CRegion;
import pprof.CRegionManager;
import java.util.*;

public class BWPlanner extends CDPPlanner {
	public BWPlanner(CRegionManager analyzer, Target target) {
		super(analyzer, target);
		bwCycleMap = new HashMap<CRegion, Long>();
		initBwMap();
	}
	
	Map<CRegion, Long> bwCycleMap;
	
	/*protected long getTotalTime() {
		return bwCycleMap.get(analyzer.getRoot());
	}*/
	
	public long getBwLimitedTime() {
		return bwCycleMap.get(analyzer.getRoot());
	}
	
	void initBwMap() {
		List<CRegion> readyList = new ArrayList<CRegion>(this.analyzer.getLeafSet());
		Set<CRegion> retiredSet = new HashSet<CRegion>();
		while (!readyList.isEmpty()) {
			CRegion current = readyList.remove(0);
			long cycle = getBwCycle(current);
			long sum = 0;
			for (CRegion child : current.getChildrenSet()) {
				sum += bwCycleMap.get(child);
			}
			if (cycle > sum)
				bwCycleMap.put(current, cycle);
			else
				bwCycleMap.put(current, sum);
			
			retiredSet.add(current);
			CRegion parent = current.getParent();
			if (parent != null && retiredSet.containsAll(parent.getChildrenSet()))
				readyList.add(parent);
		}
	}
	
	long getBwCycle(CRegion region) {
		long cacheSize = target.getCacheMB();
		long clockRateMhz = target.getClockMHz();
		long bwMB = target.getBandwidthMB();
		long dataByte = (long)(region.getAvgReadCnt() + region.getAvgWriteCnt()) * 4 - cacheSize * 1024 * 1024;
		double cycle = (double)clockRateMhz * dataByte / (double)bwMB;		
		return (long)cycle;
	}
	
	
	// TCC Processor: E550, 8MB Cache, 2.4GHz, 5.86GT/s
	// STREAMS Bench says: 25000 MB / s
	protected double getParallelTime(CRegion region) {
		double spSpeedup = (this.maxCore < region.getSelfParallelism()) ? maxCore : region.getSelfParallelism();
		double parallelTime = region.getAvgWork() / spSpeedup + overhead;
		double parallelBwTime = (double)bwCycleMap.get(region);				
		return (parallelBwTime > parallelTime) ? parallelBwTime : parallelTime;
	}	
}

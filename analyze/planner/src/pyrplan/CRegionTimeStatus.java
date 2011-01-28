package pyrplan;
import java.util.*;

import pprof.*;

public class CRegionTimeStatus {
	CRegionManager manager;
	
	Map<CRegion, Long> timeMap;
	Map<CRegion, Double> speedupMap;
	Set<CRegion> parallelSet;
	CRegion root;
	
	public CRegionTimeStatus(CRegionManager manager) {
		this.manager = manager;
		this.timeMap = new HashMap<CRegion, Long>();
		this.speedupMap = new HashMap<CRegion, Double>();
		this.parallelSet = new HashSet<CRegion>();
		this.root = manager.getRoot();
		
		for (CRegion each : manager.getCRegionSet()) {
			this.timeMap.put(each, each.getTotalWork());						
		}
	}
	
	public long peekParallelTime(CRegion region, double speedup) {
		return calculateTimeAfterParallelization(region, speedup, false);
	}
	
	public void parallelize(CRegion region, double speedup) {
		calculateTimeAfterParallelization(region, speedup, true);	
		//System.out.printf("Parallelize [%.2f] %s\n", speedup, region);
	}
	
	private long calculateTimeAfterParallelization(CRegion target, double inSpeedup, boolean update) {			
		Map<CRegion, Long> tempTimeMap = null;
		Map<CRegion, Double> tempSpeedupMap = null;
		Set<CRegion> newParallelSet = null;
		
		assert(inSpeedup >= 1.0);
		
		if (update) {
			tempTimeMap = this.timeMap;
			tempSpeedupMap = this.speedupMap;			
			newParallelSet = parallelSet;
			
		} else {
			tempTimeMap = new HashMap<CRegion, Long>(this.timeMap);
			tempSpeedupMap = new HashMap<CRegion, Double>(this.speedupMap);
			newParallelSet = new HashSet<CRegion>(parallelSet);			
		}
		
		newParallelSet.add(target);
		tempSpeedupMap.put(target, inSpeedup);
		//Set<URegion> entrySet = dManager.getDEntrySet(target);		
		List<CRegion> workList = new LinkedList<CRegion>();
		workList.add(target);
		
		//newSerialSet.addAll(entrySet);		
		
		boolean rootHandled = false;
		while (workList.isEmpty() == false) {
			CRegion current = workList.remove(0);
			//System.out.println(current);
			boolean isSerial = true;
			if (newParallelSet.contains(current))
				isSerial = false;
			
			double speedup = 1.0;
			if (!isSerial)
				speedup = tempSpeedupMap.get(current);
			
			assert(tempTimeMap != null);
			long updatedTime = estimateCRegionTime(current, speedup, tempTimeMap, isSerial);
			tempTimeMap.put(current, updatedTime);
			
			if (current == this.root) {
				rootHandled = true;
			}
			//System.out.printf("\t\ttime %d serial [%s] \n", updatedTime, isSerial);
			
			if (current.getParent() != null)
				workList.add(current.getParent());			
		}
		
		assert(this.root != null);
		assert(rootHandled == true);		
		assert(tempTimeMap != null);
		long ret = tempTimeMap.get(this.manager.getRoot());		
		//assert(ret >= this.manager.getRroot.getCriticalPath());
		return ret;
	}
	
	long estimateCRegionTime(CRegion entry, double speedup, Map<CRegion, Long> tMap, boolean serial) {
		long sum = 0;
		assert(entry != null);
		for (CRegion child : entry.getChildrenSet()) {			
			long time = tMap.get(child);
			sum += time;
		}
		
		long serialTime = entry.getExclusiveWork() + sum;		
		if (serial)
			return serialTime;	
		
		
		
		long parallelTime = (long)Math.ceil(serialTime / speedup);		
		//return (parallelTime > serialTime) ? serialTime : parallelTime;
		//assert(parallelTime <= serialTime);
		
		if (parallelTime > serialTime) {
			System.out.println("speedup = " + speedup  + " " + entry);
			//assert(false);
			return serialTime;
		}
		return parallelTime;
	}
	
	long getExecTime(CRegion region) {
		assert(timeMap != null);
		assert(region != null);
		if (!timeMap.containsKey(region)) {
			System.out.println(region + "not handled");
			assert(false);
		}
		
		return timeMap.get(region);
	}

	
	public long getExecTime() {		
		return getExecTime(this.root);
	}
}

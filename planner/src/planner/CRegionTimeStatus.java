package planner;
import java.util.*;

import kremlin.*;

/*
 * Class to manage estimated execution times for CRegions in our program.
 * This tracks which regions have been parallelized and what speedups they
 * have achieved.
 */
public class CRegionTimeStatus {
	CRegionManager manager;
	
	Map<CRegion, Long> timeMap; // mapping from region to estimated exec time
	Map<CRegion, Double> speedupMap; // mapping from region to speedup
	Set<CRegion> parallelSet; // set of regions that have been parallelized
	CRegion root; // root region (representing the whole program)
	
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
	
	/*
	 * Does a "test run" of parallelizing the region, assuming parallelization
	 * achieves the given speedup.
	 * This method returns the overall program execution time assuming this
	 * region has just been parallelized.
	 * It does not update any permanent data structures so the region still
	 * isn't counted as parallelized for planning purposes.
	 */
	public long peekParallelTime(CRegion region, double speedup) {
		return calculateTimeAfterParallelization(region, speedup, false);
	}
	
	/*
	 * Updates our data structures to indicate that the given region has been
	 * parallelized with the specified speedup.
	 */
	public void parallelize(CRegion region, double speedup) {
		calculateTimeAfterParallelization(region, speedup, true);	
		//System.out.printf("Parallelize [%.2f] %s\n", speedup, region);
	}
	
	/*
	 * Calculates the overall program execution time if the target region is
	 * parallelized and achieves a given speedup.
	 * If the update flag is set, this method also sets all pertinent data
	 * structures to indicate this region has been parallelized.
	 * If the update flag isn't set, the method only calculates what the time
	 * would be, not updating any of the permanent data structures.
	 */
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
	
	/*
	 * Returns the estimated execution time of the given region.
	 * If the serial flag is set to false, the estimated time assumes that the
	 * given region has been parallelized and achieves the speedup given as
	 * input.
	 */
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
	
	// Returns estimated execution time of the specified region.
	long getExecTime(CRegion region) {
		assert(timeMap != null);
		assert(region != null);
		if (!timeMap.containsKey(region)) {
			System.out.println(region + "not handled");
			assert(false);
		}
		
		return timeMap.get(region);
	}

	
	// Returns estimated execution time of the whole program.
	public long getExecTime() {	return getExecTime(this.root); }
}

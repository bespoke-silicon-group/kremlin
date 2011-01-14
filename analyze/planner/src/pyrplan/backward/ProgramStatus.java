package pyrplan.backward;

import java.util.*;

import pprof.*;
import pyrplan.RegionRecord;

/**
 * Contains information regarding region exec time
 * 
 * All regions are initiallized as parallel,
 * and user can change the status of each region as serial
 * and see the exec time changes.
 * 
 * @author dhjeon
 *
 */

public class ProgramStatus {
	Map<URegion, Long> timeMap;
	SRegionInfoAnalyzer anal;
	EntryManager dManager;
	SRegionManager sManager;
	Set<SRegion> serialSet;
	URegion root;

	public ProgramStatus(SRegionInfoAnalyzer anal, Set<SRegion> excludeSet) {
		this.timeMap = new HashMap<URegion, Long>();
		this.anal = anal;
		this.dManager = anal.getDManager();
		this.sManager = anal.getSManager();
		this.serialSet = new HashSet<SRegion>();
		this.root = dManager.getRoot();
		
		for (SRegion each : sManager.getSRegionSet()) {
			for (URegion dregion : dManager.getDEntrySet(each)) {
				this.timeMap.put(dregion, dregion.getCriticalPath());
			}			
		}

		for (SRegion each : excludeSet)
			serialize(each);
		
		Set<SRegion> targetSet = sManager.getSRegionSet();
		targetSet.removeAll(excludeSet);
		
		for (SRegion each : targetSet) {
			System.out.println("[Target] " + each);
		}
		
		assert(createWorkList().size() == targetSet.size());
	}
	
	long peekSerializedTime(SRegion target) {
		return calculateTimeAfterSerialization(target, false);
	}
	
	public void serialize(SRegion target) {
		long before = this.getExecTime();		
		//this.serialSet.add(target);
		calculateTimeAfterSerialization(target, true);		
		long after = this.getExecTime();
		
		//System.out.printf("[Serialize %d -> %d] %s\n", before, after, target);
	}

	private long calculateTimeAfterSerialization(SRegion target, boolean update) {
		Map<URegion, Long> tempTimeMap = null;
		Set<SRegion> newSerialSet = null;
		
		if (update) {
			tempTimeMap = this.timeMap;
			newSerialSet = serialSet;
			
		} else {
			tempTimeMap = new HashMap<URegion, Long>(this.timeMap);
			newSerialSet = new HashSet<SRegion>(serialSet);
		}
		newSerialSet.add(target);
		Set<URegion> entrySet = dManager.getDEntrySet(target);		
		List<URegion> workList = new LinkedList<URegion>();
		workList.addAll(entrySet);
		//newSerialSet.addAll(entrySet);
		
		
		boolean rootHandled = false;
		while (workList.isEmpty() == false) {
			URegion current = workList.remove(0);
			//System.out.println(current);
			boolean isSerial = false;
			if (newSerialSet.contains(current.getSRegion()))
				isSerial = true;
			
			long updatedTime = estimateUEntryTime(current, tempTimeMap, isSerial);
			tempTimeMap.put(current, updatedTime);		
			if (current.getId() == this.root.getId()) {
				rootHandled = true;
			}
			//System.out.printf("\t\ttime %d serial [%s] after processing %s\n", updatedTime, isSerial, current);
			
			for (URegion parent : current.getParentSet()) {
				if (workList.contains(parent)) {
					workList.remove(parent);					
				} 	
				workList.add(parent);
			}
		}
			
		//assert(rootHandled == true);
		assert(this.root != null);
		assert(tempTimeMap != null);
		long ret = tempTimeMap.get(this.root); 
		
		assert(ret >= this.root.getCriticalPath());
		return ret;
	}
	
	long estimateUEntryTime(URegion entry, Map<URegion, Long> tMap, boolean serial) {
		long sum = 0;
		for (URegion child : entry.getChildrenSet()) {
/*
			long cnt = entry.getChildCount(child);
			if (tMap.containsKey(child) == false) {
				System.out.println("\n" + child);
			}
			assert(tMap.containsKey(child));
			*/

			long cnt = entry.getChildCount(child);
			long time = tMap.get(child);
			sum += cnt * time;
		}
		
		long serialTime = entry.getExclusiveWork() + sum;		
		if (serial)
			return serialTime;		
		
		long parallelTime = (long)Math.ceil(serialTime / entry.getSelfParallelism());		
		//return (parallelTime > serialTime) ? serialTime : parallelTime;
		assert(parallelTime <= serialTime);
		return parallelTime;
	}
	
	long getExecTime(URegion region) {
		return timeMap.get(region);
	}

	
	long getExecTime() {
		return getExecTime(this.root);
	}
	
	List<SRegion> createWorkList() {
		List<SRegion> ret = new ArrayList<SRegion>();
		for (SRegion each : sManager.getSRegionSet()) {
			if (dManager.getDEntrySet(each).size() == 0)
				continue;
			
			else if (serialSet.contains(each))
				continue;
			
			else
				ret.add(each);
		}
		
		return ret;
	}
	
	//SRegion getMinImpactRegion(List<SRegion> candidates, Set<URegion> serialSet) {
	public RegionRecord getMinImpactRegion() {
		//List<SRegion> worklist = new ArrayList<SRegion>(candidates);
		
		List<SRegion> worklist = createWorkList();
		long min = getExecTime();
		SRegion minRegion = null;
		long before = getExecTime();
		
		while (worklist.isEmpty() == false) {
			SRegion current = worklist.remove(0);
			
			//long updated = this.getSerializedTime(current, serialSet, false);
			long updated = this.peekSerializedTime(current);
			//System.out.printf("Time: %d, MinRegion: %s\n", updated, current);
			assert(updated >= this.root.getCriticalPath());
			
			if (minRegion == null || updated <= min) {
				min = updated;
				minRegion = current;
			}
		}
		//System.out.println(minRegion);
		System.out.printf("\nSerialization [%d -> %d] MinRegion: %s %d\n", before, min, minRegion, this.root.getWork());
		double reducePercent = ((double)(min - before) / this.root.getWork()) * 100.0;
		if (minRegion == null)
			return null;
		
		SRegionInfo info = anal.getSRegionInfo(minRegion);
		RegionRecord ret = new RegionRecord(info);
		ret.setTimeSaving(reducePercent);
		System.err.println(ret);
		
		return ret;
	}
	
	
}
 
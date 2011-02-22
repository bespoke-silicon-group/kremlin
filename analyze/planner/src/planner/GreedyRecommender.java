package planner;

import java.util.*;

import pprof.RegionStatus;
import pprof.SRegion;
import pprof.SRegionInfo;
import pprof.SRegionInfoAnalyzer;
import pprof.SRegionInfoGroup;
import pprof.SRegionManager;
import pprof.URegion;
import pprof.URegionManager;

public class GreedyRecommender {
	SRegionManager sManager;
	URegionManager dManager;
	Map<URegion, Long> timeMap;
	URegion root;	
	SRegionInfoAnalyzer analyzer;
	Set<SRegion> excludeSet;
	Set<SRegion> parallelSet;
	
	public GreedyRecommender(SRegionManager sManager, URegionManager dManager) {
		this.sManager = sManager;
		this.dManager = dManager;
		this.timeMap = new HashMap<URegion, Long>();
		this.root = dManager.getRoot();
		this.analyzer = dManager.getSRegionAnalyzer(); 
		this.excludeSet = new HashSet<SRegion>();
		this.parallelSet = new HashSet<SRegion>();		
	}
	
	long getDEntryTime(URegion entry, Map<URegion, Long> tMap, boolean parallel) {
		long sum = 0;
		for (URegion child : entry.getChildrenSet()) {
			long cnt = entry.getChildCount(child);
			long time = tMap.get(child);
			sum += cnt * time;
		}
		
		long serialTime = entry.getExclusiveWork() + sum;		
		if (!parallel)
			return serialTime;
		
		long max = 0;		
		for (URegion child : entry.getChildrenSet()) {
			long diff = tMap.get(child) - child.getCriticalPath();
			if (diff > max)
				max = diff;
		}
		
		long parallelTime = (long)(serialTime / entry.getSelfParallelism());
		return (parallelTime > serialTime) ? serialTime : parallelTime;
	}
	
	long getParallelizedTime(SRegion target, Set<URegion> parallelSet, boolean update) {
		//System.out.println("\nSerializing " + target);
		
		//assert(map.containsKey(target));		
		Map<URegion, Long> tempTimeMap = null;
		Set<URegion> newParallelSet = null;
		
		if (update) {
			tempTimeMap = this.timeMap;
			newParallelSet = parallelSet;
			
		} else {
			tempTimeMap = new HashMap<URegion, Long>(this.timeMap);
			newParallelSet = new HashSet<URegion>(parallelSet);
		}
		
		Set<URegion> entrySet = dManager.getDEntrySet(target);		
		List<URegion> workList = new LinkedList<URegion>();
		workList.addAll(entrySet);
		newParallelSet.addAll(entrySet);
		
		
		boolean rootHandled = false;
		while (workList.isEmpty() == false) {
			URegion current = workList.remove(0);
			//System.out.println(current);
			boolean isParallel = false;			
			if (newParallelSet.contains(current))
				isParallel = true;
			
			long updatedTime = getDEntryTime(current, tempTimeMap, isParallel);
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
			
		assert(rootHandled == true);
		long ret = tempTimeMap.get(this.root); 
		//System.out.printf("\ttime %d  sregion %s\n", ret, target);
		//assert(temp >= this.root.cp);
		assert(ret >= this.root.getCriticalPath());
		return ret;
	}
	

	
	SRegion getMaxImpactRegion(List<SRegion> candidates, Set<URegion> serialSet) {
		List<SRegion> worklist = new ArrayList<SRegion>(candidates);
		long min = this.root.getWork();
		SRegion minRegion = null;
		
		while (worklist.isEmpty() == false) {
			SRegion current = worklist.remove(0);
			long updated = this.getParallelizedTime(current, serialSet, false);
			//System.out.printf("Time: %d, MinRegion: %s\n", updated, current);
			assert(updated >= this.root.getCriticalPath());
			
			if (updated < min) {
				min = updated;
				minRegion = current;
			}
		}
		return minRegion;
	}
	
	
	List<SRegion> filterCandidates(List<SRegion> candidates, double threshold) {
		List<SRegion> ret = new ArrayList<SRegion>();		
		for (SRegion candidate : candidates) {
			SRegionInfo info = analyzer.getSRegionInfo(candidate);
			long diff = info.getTotalWork() - (long)(info.getTotalWork() / info.getTotalParallelism());
			if (diff <= (this.root.getWork() * threshold)) {
				ret.add(candidate);
			}
		}
		
		System.out.printf("[Filter thresh %f] # of regions filtered %d / %d, %d left\n", 
				threshold, ret.size(), candidates.size(), candidates.size() - ret.size());
		return ret;
	}
	
	void initTimeMap() {
		for (SRegion region : sManager.getSRegionSet()) {
			for (URegion entry : dManager.getDEntrySet(region))
				timeMap.put(entry, entry.getWork());
		}
	}
	
	
	public RecList recommend(double filterThreshold, Set<SRegion> exSet) {
		RecList ret = new RecList();
		
		this.excludeSet = new HashSet<SRegion>(exSet);
		//System.out.println("Root:");
		//System.out.println("\t" + this.root);
		//this.root.dumpChildren();
		initTimeMap();
		List<SRegion> candidates = new ArrayList<SRegion>(sManager.getSRegionSet());
		Set<SRegion> toRemove = new HashSet<SRegion>();
		for (SRegion region : candidates) {
			if (dManager.getDEntrySet(region).size() == 0)
				toRemove.add(region);			
		}
		candidates.removeAll(toRemove);
					
		Set<URegion> serialSet = new HashSet<URegion>();
		
		// remove excluded set
		candidates.removeAll(this.excludeSet);

		// filter small regions
		List<SRegion> filtered = filterCandidates(candidates, filterThreshold);
		candidates.removeAll(filtered);

		long initialTime = timeMap.get(dManager.getRoot());
		/*
		for (SRegion each : filtered) {
			initialTime = this.getSerializedTime(each, serialSet, true);
			System.out.printf("[Filter] Time: %d, MinRegion: %s\n", initialTime, each);
		}*/
		
		while (candidates.size() > 0) {
			SRegion target = getMaxImpactRegion(candidates, serialSet);			
			System.out.println(target);
			candidates.remove(target);
			SRegionInfo targetInfo = analyzer.getSRegionInfo(target);
			assert(targetInfo.getRegionStatus() == RegionStatus.SERIAL);
			
			Set<SRegion> suppressed = getSuppressedRegions(target);
			for (SRegion each : suppressed) {
				SRegionInfo info = analyzer.getSRegionInfo(each);
				if (info.getRegionStatus() == RegionStatus.SERIAL &&
						candidates.contains(each))
					System.out.println("\tSuppressing " + each);
					info.setRegionStatus(RegionStatus.SUPPRESSED);
			}				
			candidates.removeAll(suppressed);
			
			long updated = this.getParallelizedTime(target, serialSet, true);
			//System.out.printf("Time: %d, MinRegion: %s\n", updated, target);
			
			ret.appendRecUnit(target, updated, initialTime);
			initialTime = updated;
		}
		
		return ret;
	}		
	
	Set<SRegion> getSuppressedRegions(SRegion region) {
		Set<SRegion> ret = new HashSet<SRegion>();
		SRegionInfo info = analyzer.getSRegionInfo(region);
		SRegionInfoGroup groupInfo = analyzer.getRegionGroup(region);
		
		// add group members
		for (int i=0; i<groupInfo.size(); i++) {
			SRegionInfo each = groupInfo.getEntry(i);
			if (each != info)
				ret.add(each.getSRegion());
		}
		
		Set<SRegion> descendents = analyzer.getDescendantSet(region);
		ret.addAll(descendents);
		
		Set<SRegion> ancestors = analyzer.getAncestorSet(region);
		ret.addAll(ancestors);		
		
		return ret;
	}
}


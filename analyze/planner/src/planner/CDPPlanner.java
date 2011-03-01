package planner;


import java.util.*;

import planner.*;
import pprof.*;

public class CDPPlanner {
	CRegionManager analyzer;
	Map<CRegion, Double> pointMap = new HashMap<CRegion, Double>();
	Map<CRegion, Set<CRegion>> setMap = new HashMap<CRegion, Set<CRegion>>();
	int maxCore;
	int overhead;
	Target target;
	
	public CDPPlanner(CRegionManager analyzer, Target target) {		
		this.analyzer = analyzer;
		this.pointMap = new HashMap<CRegion, Double>();
		this.setMap = new HashMap<CRegion, Set<CRegion>>();
		//this.filterFile = file;	
		this.target = target;
		this.maxCore = target.numCore;
		this.overhead = target.overhead;
	}
	
	protected double getParallelTime(CRegion region) {
		double spSpeedup = (this.maxCore < region.getSelfParallelism()) ? maxCore : region.getSelfParallelism();
		double parallelTime = region.getAvgWork() / spSpeedup + overhead;
		return parallelTime;
	}
	
	public Plan plan(Set<CRegion> toExclude) {		
		Set<CRegion> postFilterSet = toExclude;
		List<CRegion> list = new ArrayList<CRegion>();
		Set<CRegion> retired = new HashSet<CRegion>();
		
		for (CRegion each : analyzer.getCRegionSet()) {
			//System.out.println(each);
			if (each.getChildrenSet().size() == 0) {
				list.add(each);
			}
		}
		CRegion root = analyzer.getRoot();
		assert(root != null);
		/*
		for (SRegionInfo each : postFilterSet) {
			System.out.println("! " + each);
		}*/
		
		while(list.isEmpty() == false) {
			CRegion current = list.remove(0);
			SRegion region = current.getSRegion();
			
			retired.add(current);			
			
			double sum = 0;
			for (CRegion child : current.getChildrenSet()) {
				sum += pointMap.get(child);
			}
			
			boolean exclude = false;
			//double selfPoint = this.speedupCalc.getAppTimeReduction(current, root);			 
			
			double parallelTime = this.getParallelTime(current);
			double speedup = current.getAvgWork() / parallelTime;
			double coverage = ((double)current.getTotalWork() / (double)root.getTotalWork()) * 100.0;
			double selfPoint = coverage - coverage / speedup;
					 
			if (toExclude.contains(current))
				exclude = true;
			
			if (exclude) {				
				selfPoint = 0.0;
			} else {
				//assert(current.getSRegion().getType() == RegionType.LOOP);
			}
			
			//if (!exclude)
				//System.out.printf("[Point: %.2f] %s\n", selfPoint, current.getSRegion());
			
			Set<CRegion> bestSet = new HashSet<CRegion>();
			//if (selfPoint * filter.getOuterIncentive() > sum) {			
			if (selfPoint  > sum) {
				bestSet.add(current);
				pointMap.put(current, selfPoint);
				
			} else {
				Set<CRegion> union = getChildrenUnionSet(current);
				bestSet.addAll(union);
				pointMap.put(current, sum);
			}

			setMap.put(current, bestSet);
			//System.out.println("Current: " + current.getSRegion());
			
			// add parent if its children are all examined
			CRegion parent = current.getParent();
			if (parent != null) {
				assert(retired.contains(parent) == false);
				
				Set<CRegion> children = parent.getChildrenSet();			
				if (retired.containsAll(children)) {
					list.add(parent);
				}	
			}
									
		}		
			

		assert(setMap.containsKey(root));
		Set<CRegion> set = setMap.get(root);
		List<CRegionRecord> ret = new ArrayList<CRegionRecord>();
		for (CRegion each : set) {
			CRegionRecord toAdd = new CRegionRecord(each, maxCore, pointMap.get(each));
			ret.add(toAdd);			
		}
		
		//List<CRegion> infoList = SRegionInfoFilter.toSRegionInfoList(analyzer, ret);
		Collections.sort(ret);		
		double sum = 0.0;				
		Plan plan = new Plan(ret, this.target, pointMap.get(root));		
		return plan;
	}
	
	
	Set<CRegion> getChildrenUnionSet(CRegion info) {
		Set<CRegion> children = info.getChildrenSet();
		Set<CRegion> ret = new HashSet<CRegion>();
		for (CRegion child : children) {
			assert(setMap.containsKey(child));
			ret.addAll(setMap.get(child));
		}		
		return ret;
		
	}
}

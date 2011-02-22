package pyrplan.omp;


import java.util.*;

import pprof.*;
import pyrplan.*;

public class CDPPlanner {
	CRegionManager analyzer;
	Map<CRegion, Double> pointMap = new HashMap<CRegion, Double>();
	Map<CRegion, Set<CRegion>> setMap = new HashMap<CRegion, Set<CRegion>>();
	int maxCore;
	int overhead;
	
	public CDPPlanner(CRegionManager analyzer, int maxCore, int overhead) {		
		this.analyzer = analyzer;
		this.pointMap = new HashMap<CRegion, Double>();
		this.setMap = new HashMap<CRegion, Set<CRegion>>();
		//this.filterFile = file;		
		this.maxCore = maxCore;
		this.overhead = overhead;
	}
	
	public double plan(Set<CRegion> toExclude) {		
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
			 
			double spSpeedup = (this.maxCore < current.getSelfParallelism()) ? maxCore : current.getSelfParallelism();
			double parallelTime = current.getAvgWork() / spSpeedup + overhead;
			double speedup = current.getAvgWork() / parallelTime;
			double coverage = ((double)current.getTotalWork() / (double)root.getTotalWork()) * 100.0;
			double selfPoint = coverage - coverage / speedup;
			
			
			/* 

			if (!postFilterSet.contains(current)) {
				exclude = true;
			}*/
			 
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
			
		//System.out.println(root);
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
		
		for (CRegionRecord each : ret) {
			System.out.println(each);
		}
		//System.out.println("\n\n\n");
		/*
		for (CRegion each : ret) {
			//RegionRecord toAdd = new RegionRecord(each, 1, 1.0);
			double timeSave = (double)(pointMap.get(each));
			sum += timeSave;
			//toAdd.setExpectedExecTime((long)timeSave);
			//retList.add(toAdd);
			System.out.printf("%.2f: %s\n", timeSave, each);
		}*/
		
		
		
		//System.out.printf("total time save = %.2f root time = %.2f\n", sum, pointMap.get(root));
				
		return pointMap.get(root);
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

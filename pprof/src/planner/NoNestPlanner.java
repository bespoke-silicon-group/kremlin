package planner;

import java.util.*;
import kremlin.*;

public class NoNestPlanner {
	CRegionManager analyzer;
	Map<CRegion, Double> pointMap = new HashMap<CRegion, Double>();
	Map<CRegion, Set<CRegion>> setMap = new HashMap<CRegion, Set<CRegion>>();
	int maxCore;
	int overhead;
	Target target;
	CRegion root;
	
	public NoNestPlanner(CRegionManager analyzer, Target target) {		
		this.analyzer = analyzer;
		this.pointMap = new HashMap<CRegion, Double>();
		this.setMap = new HashMap<CRegion, Set<CRegion>>();
		//this.filterFile = file;	
		this.target = target;
		this.maxCore = target.numCore;
		this.overhead = target.overhead;
		this.root = analyzer.getRoot();
	}
	
	CRegion getRoot() {
		return this.root;
	}
	
	Set<CRegion> getParallelRegionSet(CRegion target) {
		assert(setMap.containsKey(target));
		return setMap.get(target);
	}
	
	double getSavedTime(CRegion region) {
		return pointMap.get(region);
	}
	
	void setParallelRegionSet(CRegion target, Set<CRegion> set, double timeSaved) {
		setMap.put(target, set);		
		pointMap.put(target, timeSaved);
	}
	
	protected double getSerialTime(CRegion region) {
		return region.getAvgWork();
	}
	
	protected double getParallelTime(CRegion region) {
		double spSpeedup = (this.maxCore < region.getSelfP()) ? maxCore : region.getSelfP();
		double parallelTime = region.getAvgWork() / spSpeedup + overhead;		
		return parallelTime;
	}
	
	double getCoverage(CRegion region) {
		CRegion root = analyzer.getRoot();
		double totalTime = this.getSerialTime(root);		
		double regionTime = (double)region.getTotalWork();
		double ret = (regionTime * 100.0) / totalTime; 
		
		assert(ret >= 0.0 && ret <= 100.0);
		return 	ret;
	}
	
	double getSpeedup(CRegion region) {		
		double parallelTime = this.getParallelTime(region);		
		double serialTime = this.getSerialTime(region);
		double speedup = serialTime / parallelTime;
		if (parallelTime > serialTime)
			speedup = 1.0;
		return speedup;
	}
	
	double getSelfPoint(CRegion current, Set<CRegion> excludeSet) {
		if (excludeSet.contains(current))
			return 0.0;
		
		if (current.getRecursiveType() == CRecursiveType.REC_SINK)
			return 0.0;
		
		double speedup = getSpeedup(current);		
		double coverage = getCoverage(current);
		double selfPoint = coverage - coverage / speedup;
		//System.err.printf("pTime = %.2f, sTime = %.2f, coverage = %.2f, speedup = %.2f, sPoint = %.2f\n", parallelTime, serialTime, coverage, speedup, selfPoint);
		//System.err.printf("[SelfPoint] Region: %s, Cov: %.2f, Speedup: %.2f, SelfPoint: %.2f\n", 
			//current, coverage, speedup, selfPoint);
		if (selfPoint <= -0.0)
			selfPoint = 0.0;
		
		assert(selfPoint <= 100.0);	// percentage
		return selfPoint;
	}
	
	double getChildrenPoint(CRegion region) {
		double sum = 0;
		for (CRegion child : region.getChildrenSet()) {
			sum += pointMap.get(child);
		}
		return sum;
	}

	boolean isReady(CRegion region, Set<CRegion> retired) {
		if (region == null)
			return false;

		assert(retired.contains(region) == false);
		Set<CRegion> children = region.getChildrenSet();
		if (retired.containsAll(children))
			return true;
		else
			return false;
	}
 	
	public Plan plan(Set<CRegion> toExclude) {		
		Set<CRegion> postFilterSet = toExclude;
		List<CRegion> list = new ArrayList<CRegion>();
		Set<CRegion> retired = new HashSet<CRegion>();
		
		// bottom-up evaluation: start with leaf regions		
		list.addAll(analyzer.getLeafSet());	
		
		/*
		for (SRegionInfo each : postFilterSet) {
			System.out.println("! " + each);
		}*/
		
		while(list.isEmpty() == false) {			
			CRegion current = list.remove(0);			
			retired.add(current);			
			
			double childrenPoint = getChildrenPoint(current);
			
			double selfPoint = getSelfPoint(current, toExclude);
			
			double bestPoint;
			Set<CRegion> bestSet = new HashSet<CRegion>();			 

			if (selfPoint > childrenPoint) {				
				bestSet.add(current);
				bestPoint = selfPoint;				

			} else {				
				Set<CRegion> parallelSet = getChildrenUnionSet(current); 
				bestSet.addAll(parallelSet);
				bestPoint = childrenPoint;				
			}
			setParallelRegionSet(current, bestSet, bestPoint);
			//System.out.println("Current: " + current.getSRegion());
			
			// add parent if its children are all examined
			CRegion parent = current.getParent();
			if (isReady(parent, retired))
				list.add(parent);	
			
		}
		
		assert(setMap.containsKey(root));
		//Set<CRegion> set = setMap.get(root);
		Set<CRegion> set = getParallelRegionSet(root);
		List<CRegionRecord> ret = new ArrayList<CRegionRecord>();
		double coverageSum = 0.0;
		for (CRegion each : set) {
			double timeSaved = getSavedTime(each);
			double coverage = getCoverage(each); 
			coverageSum += coverage;
			assert(coverageSum <= 100.0);
			//System.err.printf("[Print] TimeSavedTime: %.2f, Cov: %.2f, Region: %s\n", timeSaved, coverage, each);
			CRegionRecord toAdd = new CRegionRecord(each, maxCore, timeSaved);
			ret.add(toAdd);
		}

		//List<CRegion> infoList = SRegionInfoFilter.toSRegionInfoList(analyzer, ret);
		Collections.sort(ret);
		double sum = 0.0;
		Plan plan = new Plan(ret, this.target, pointMap.get(root), this.getSerialTime(root));		
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

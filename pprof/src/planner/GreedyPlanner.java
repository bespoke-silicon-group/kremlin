package planner;

import java.util.*;
import kremlin.CRegion;
import kremlin.CRegionManager;
import kremlin.SRegion;

public class GreedyPlanner {
	CRegionManager analyzer;
	Map<CRegion, Double> pointMap = new HashMap<CRegion, Double>();
	Map<CRegion, Set<CRegion>> setMap = new HashMap<CRegion, Set<CRegion>>();
	int maxCore;
	int overhead;
	Target target;
	
	public GreedyPlanner(CRegionManager analyzer, Target target) {		
		this.analyzer = analyzer;
		this.pointMap = new HashMap<CRegion, Double>();
		this.setMap = new HashMap<CRegion, Set<CRegion>>();
		//this.filterFile = file;	
		this.target = target;
		this.maxCore = target.numCore;
		this.overhead = target.overhead;
		
		initPointMap();
	}	
	
	void initPointMap() {
		for (CRegion each : analyzer.getCRegionSet()) {
			pointMap.put(each, getPoint(each));
		}
	}
	
	double getPoint(CRegion current) {
		double parallelTime = this.getParallelTime(current);
		double serialTime = this.getSerialTime(current);
		double speedup = serialTime / parallelTime;
		if (parallelTime > serialTime)
			speedup = 1.0;
		double coverage = ((this.getSerialTime(current) * current.getInstanceCount()) / (double)this.getSerialTime(analyzer.getRoot())) * 100.0;			
		double selfPoint = coverage - coverage / speedup;		
		return selfPoint;
	}
	
	protected double getSerialTime(CRegion region) {
		return region.getAvgWork();
	}
	
	protected double getParallelTime(CRegion region) {
		double spSpeedup = (this.maxCore < region.getSelfP()) ? maxCore : region.getSelfP();
		double parallelTime = region.getAvgWork() / spSpeedup + overhead;
		return parallelTime;
	}
	
	List<CRegion> getSortedCRegionList() {
		List<CRegion> ret = new ArrayList<CRegion>(analyzer.getCRegionSet());
		class Sorter implements Comparator {

			@Override
			public int compare(Object arg0, Object arg1) {
				CRegion r0 = (CRegion)arg0;
				CRegion r1 = (CRegion)arg1;
				double diff = pointMap.get(r0) - pointMap.get(r1);
				if (diff == 0.0)
					return 0;
				else if (diff > 0.0)
					return -1;
				else
					return 1;				
			}			
		}
		Collections.sort(ret, new Sorter());
		return ret;
	}
	
	public Plan plan(Set<CRegion> toExclude) {		
		Set<CRegion> postFilterSet = toExclude;
		List<CRegion> list = getSortedCRegionList();		
		Set<CRegion> retired = new HashSet<CRegion>(toExclude);
		
		
		CRegion root = analyzer.getRoot();
		assert(root != null);
		/*
		for (SRegionInfo each : postFilterSet) {
			System.out.println("! " + each);
		}*/
		double total = 0.0;
		List<CRegionRecord> ret = new ArrayList<CRegionRecord>();
		for (CRegion region : list) {
			if (!retired.contains(region)) {
				ret.add(new CRegionRecord(region, maxCore, pointMap.get(region)));
				total += pointMap.get(region);
				
				Set<CRegion> excludeSet = getExcludeSet(region);
				/*for (CRegion each : excludeSet) {
					System.out.println("\t" + each);				
				}*/
				retired.addAll(excludeSet);
			}						
		}	
						
		Plan plan = new Plan(ret, this.target, total, this.getSerialTime(root));		
		return plan;
	}	

	
	Set<CRegion> getExcludeSet(CRegion region) {
		Set<CRegion> ret = new HashSet<CRegion>();
		// ancestors
		CRegion current = region.getParent();
		while (current != null) {
			ret.add(current);
			current = current.getParent();
		}
		// descendents
		List<CRegion> readyList = new ArrayList<CRegion>();
		readyList.addAll(region.getChildrenSet());
		while (!readyList.isEmpty()) {
			current = readyList.remove(0);
			ret.add(current);
			readyList.addAll(current.getChildrenSet());
		}
		
		return ret;
	}
}

package pyrplan.omp;

import java.io.File;

import java.util.*;
import pprof.*;
import pprof.SRegionSpeedupCalculator.LimitFactor;
import pprof.SRegionSpeedupCalculator.ScaleMode;
import pyrplan.*;
import runner.PyrProfRunner;


public class DPPlanner extends Planner {
	SRegionInfoAnalyzer analyzer;
	Map<SRegion, Double> pointMap;
	Map<SRegion, Set<SRegion>> setMap;
	String filterFile;
	SRegionSpeedupCalculator speedupCalc;
	
	public DPPlanner(SRegionInfoAnalyzer analyzer) {
		super(analyzer);
		this.analyzer = analyzer;
		this.pointMap = new HashMap<SRegion, Double>();
		this.setMap = new HashMap<SRegion, Set<SRegion>>();
		//this.filterFile = file;
		this.speedupCalc = new SRegionSpeedupCalculator(ScaleMode.LINEAR, LimitFactor.ALL, 
				ParameterSet.minWorkChunk, ParameterSet.bwThreshold);
	}
		
	
	Set<SRegion> getChildrenUnionSet(SRegionInfo info) {
		Set<SRegion> children = info.getChildren();
		Set<SRegion> ret = new HashSet<SRegion>();
		for (SRegion child : children) {
			assert(setMap.containsKey(child));
			ret.addAll(setMap.get(child));
		}
		
		return ret;
		
	}
	
	
	/** 
	 * use self parallelism in choosing regions to parallelize
	 * a weight value is used to give favor for outer regions
	 **/
	    
	public List<RegionRecord> plan(FilterControl filter) {		
		//System.out.printf("DP Planner Out MIN_SPEEDUP=%.3f MIN_SP=%.3f OUTER_INCENTIVE=%.2f\n", minSpeedup, minSP, outerIncentive);
		
		//Set<SRegion> excludeSet = PyrProfRunner.createExcludeSet(analyzer.getSManager(), analyzer, filter, filterFile);		
		this.filterFile = filter.getFileName();
		Set<SRegionInfo> postFilterSet = filter.getPostFilterSRegionInfoSet(analyzer);
		List<SRegionInfo> list = new ArrayList<SRegionInfo>();
		Set<SRegion> retired = new HashSet<SRegion>(); 
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			if (each.getChildren().size() == 0) {
				list.add(each);
			}
		}
		SRegionInfo root = analyzer.getRootInfo();
		
		for (SRegionInfo each : postFilterSet) {
			System.out.println("! " + each);
		}
		
		while(list.isEmpty() == false) {
			SRegionInfo current = list.remove(0);
			SRegion region = current.getSRegion();
			
			retired.add(current.getSRegion());			
			
			double sum = 0;
			for (SRegion child : current.getChildren()) {
				sum += pointMap.get(child);
			}
			
			boolean exclude = false;
			double selfPoint = this.speedupCalc.getAppTimeReduction(current, root);
			 

			if (!postFilterSet.contains(current)) {
				exclude = true;
			}
			if (exclude) {				
				selfPoint = 0.0;
			} 
			
			//if (!exclude)
				System.out.printf("[Point: %.2f] %s\n", selfPoint, current.getSRegion());
			
			Set<SRegion> bestSet = new HashSet<SRegion>();
			if (selfPoint * filter.getOuterIncentive() > sum) {			
				bestSet.add(current.getSRegion());
				pointMap.put(current.getSRegion(), selfPoint);
				
			} else {
				Set<SRegion> union = getChildrenUnionSet(current);
				bestSet.addAll(union);
				pointMap.put(current.getSRegion(), sum);
			}

			setMap.put(current.getSRegion(), bestSet);
			//System.out.println("Current: " + current.getSRegion());
			
			// add parent if its children are all examined
			for (SRegion parent : current.getParents()) {
				assert(retired.contains(parent) == false);
				SRegionInfo info = analyzer.getSRegionInfo(parent);
				Set<SRegion> children = info.getChildren();
				//System.out.println("\tParent: " + parent);
				/*
				for (SRegion child : children) {
					System.out.println("\t\t" + child + " " + retired.contains(child));
				}*/
				if (retired.containsAll(children)) {
					list.add(info);
				}
			}			
		}		
			
		SRegionInfo rootInfo = analyzer.getRootInfo();
		assert(setMap.containsKey(rootInfo.getSRegion()));
		Set<SRegion> set = setMap.get(rootInfo.getSRegion());		
		List<SRegion> ret = new ArrayList<SRegion>(set);
		List<SRegionInfo> infoList = SRegionInfoFilter.toSRegionInfoList(analyzer, ret);
		
		List<RegionRecord> retList = new ArrayList<RegionRecord>();
		SRegionInfoSorter.sortBySpeedup(infoList);
		for (SRegionInfo each : infoList) {
			RegionRecord toAdd = new RegionRecord(each);
			toAdd.setTimeSaving(pointMap.get(each.getSRegion()));
			retList.add(toAdd);
		}
		
		
		if (pointMap.get(rootInfo.getSRegion()) == 0.0)
			infoList = new ArrayList<SRegionInfo>();
		this.setPlan(infoList);
		
		
		return retList;
	}
	
	double getTimeSavingPercent() {
		SRegionInfo rootInfo = analyzer.getRootInfo();		
		return pointMap.get(rootInfo.getSRegion());
	}
}

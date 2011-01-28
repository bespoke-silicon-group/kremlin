package pyrplan.omp;

import java.io.File;

import java.util.*;

import pprof.*;
import pprof.SRegionSpeedupCalculator.LimitFactor;
import pprof.SRegionSpeedupCalculator.ScaleMode;
import pyrplan.*;
import runner.OMPGrepReader;
import runner.PyrProfRunner;
import runner.SRegionProfileAnalyzer;


public class DPPlanner extends Planner {
	SRegionInfoAnalyzer analyzer;
	Map<SRegion, Double> pointMap;
	Map<SRegion, Set<SRegion>> setMap;
	String filterFile;
	SRegionSpeedupCalculator speedupCalc;
	int maxCore;
	int overhead;
	
	public DPPlanner(SRegionInfoAnalyzer analyzer, int maxCore, int overhead) {
		super(analyzer);
		this.analyzer = analyzer;
		this.pointMap = new HashMap<SRegion, Double>();
		this.setMap = new HashMap<SRegion, Set<SRegion>>();
		//this.filterFile = file;
		this.speedupCalc = new SRegionSpeedupCalculator(ScaleMode.LINEAR, LimitFactor.SP, 
				ParameterSet.minWorkChunk, ParameterSet.bwThreshold);
		this.maxCore = maxCore;
		this.overhead = overhead;
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
	    
	public List<RegionRecord> plan(Set<SRegionInfo> toExclude) {		
		//System.out.printf("DP Planner Out MIN_SPEEDUP=%.3f MIN_SP=%.3f OUTER_INCENTIVE=%.2f\n", minSpeedup, minSP, outerIncentive);
		
		//Set<SRegion> excludeSet = PyrProfRunner.createExcludeSet(analyzer.getSManager(), analyzer, filter, filterFile);		
		//this.filterFile = filter.getFileName();
		//Set<SRegionInfo> postFilterSet = filter.getPostFilterSRegionInfoSet(analyzer);
		Set<SRegionInfo> postFilterSet = toExclude;
		List<SRegionInfo> list = new ArrayList<SRegionInfo>();
		Set<SRegion> retired = new HashSet<SRegion>(); 
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			if (each.getChildren().size() == 0) {
				list.add(each);
			}
		}
		SRegionInfo root = analyzer.getRootInfo();
		/*
		for (SRegionInfo each : postFilterSet) {
			System.out.println("! " + each);
		}*/
		
		while(list.isEmpty() == false) {
			SRegionInfo current = list.remove(0);
			SRegion region = current.getSRegion();
			
			retired.add(current.getSRegion());			
			
			double sum = 0;
			for (SRegion child : current.getChildren()) {
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
			
			Set<SRegion> bestSet = new HashSet<SRegion>();
			//if (selfPoint * filter.getOuterIncentive() > sum) {			
			if (selfPoint  > sum) {
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
			RegionRecord toAdd = new RegionRecord(each, 1, 1.0);
			double timeSave = (double)(pointMap.get(each.getSRegion()));
			toAdd.setExpectedExecTime((long)timeSave);
			retList.add(toAdd);
			//System.out.println(toAdd);
		}
		
		
		if (pointMap.get(rootInfo.getSRegion()) == 0.0)
			infoList = new ArrayList<SRegionInfo>();
		this.setPlan(infoList);
		
		
		return retList;
	}
	
	public double getTimeSavingPercent() {
		SRegionInfo rootInfo = analyzer.getRootInfo();		
		return pointMap.get(rootInfo.getSRegion());
	}
	
	public static void main(String args[]) {
		String dir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-b";
		String project = "is";
		String rawDir = dir + "/" + project;
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";

		
		SRegionManager sManager = new SRegionManager(new File(sFile), false);		
		URegionManager dManager = new URegionManager(sManager, new File(dFile), true);	
		
		OMPGrepReader reader = new OMPGrepReader();		
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		profileAnalyzer.dump(rawDir + "/analysis.txt");			
		
		int core = 8;
		getSpeedup(analyzer, core, 0);
		System.out.println("\n");
		getSpeedup(analyzer, 16, 0);
		System.out.println("\n");
		getSpeedup(analyzer, 32, 0);
		
	}
	static Set<SRegionInfo> getBodyFuncSet(SRegionInfoAnalyzer analyzer) {
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			if (each.getSRegion().getType() != RegionType.LOOP)
				ret.add(each);
		}
		return ret;
	}
	static double getSpeedup(SRegionInfoAnalyzer analyzer, int core, int overhead) {
		DPPlanner dp = new DPPlanner(analyzer, core, overhead);
		Set<SRegionInfo> filter0 = getBodyFuncSet(analyzer);
		dp.plan(filter0);
		double savePercent = dp.getTimeSavingPercent();
		double speedup = 100.0 / (100.0 - savePercent);
		return speedup;
	}
}

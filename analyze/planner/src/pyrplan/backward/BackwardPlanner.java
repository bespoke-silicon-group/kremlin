package pyrplan.backward;

import java.util.*;

import pprof.*;
import pyrplan.FilterControl;
import pyrplan.Planner;
import pyrplan.RegionRecord;
import pyrplan.SRegionInfoFilter;

public class BackwardPlanner extends Planner {
	SRegionInfoAnalyzer analyzer;
	Map<SRegion, Double> pointMap;
	Map<SRegion, Set<SRegion>> setMap;	
	SRegionSpeedupCalculator speedupCalc;
	
	//Set<SRegion> excludeSet;
	
	public BackwardPlanner(SRegionInfoAnalyzer analyzer) {
		super(analyzer);
		this.analyzer = analyzer;
		this.pointMap = new HashMap<SRegion, Double>();
		this.setMap = new HashMap<SRegion, Set<SRegion>>();
		
		//this.speedupCalc = new SRegionSpeedupCalculator(ScaleMode.LINEAR, LimitFactor.ALL, 100.0);
		
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
	
	private Set<SRegion> getExcludeSet(SRegionInfoAnalyzer analyzer, FilterControl filter) {		
		SRegionManager sManager = analyzer.getSManager();
		double minDOALL = filter.getMinDOALL();
		double minDOACROSS = filter.getMinDOACROSS();
		double minSP = filter.getMinSP();
		

		Set<SRegion> loopSet = sManager.getSRegionSet();
		Set<SRegionInfo> loopInfoSet = SRegionInfoFilter.toSRegionInfoSet(analyzer, loopSet);
		Set<SRegionInfo> filteredInfoSet = SRegionInfoFilter.filterMinSpeedupAllLoop(loopInfoSet, minDOALL, minDOACROSS, minSP);
		String file = filter.getFileName();
		List<SRegionInfo> doAllInfoSet = SRegionInfoFilter.filterExcludedRegions(analyzer, new ArrayList<SRegionInfo>(filteredInfoSet), file);
		List<SRegion> filteredSet = SRegionInfoFilter.toSRegionList(doAllInfoSet);
		Set<SRegion> ret = sManager.getSRegionSet();
		ret.removeAll(filteredSet);	
		//Set<SRegion> ret = new HashSet<SRegion>();
		return ret;
	}
		
	/** 
	 * use self parallelism in choosing regions to parallelize
	 * a weight value is used to give favor for outer regions
	 **/
	    
	public List<RegionRecord> plan(FilterControl filter) {		
		//System.out.printf("DP Planner Out MIN_SPEEDUP=%.3f MIN_SP=%.3f OUTER_INCENTIVE=%.2f\n", minSpeedup, minSP, outerIncentive);
		Set<SRegion> excludeSet = getExcludeSet(analyzer, filter);
		ProgramStatus status = new ProgramStatus(analyzer, excludeSet);		
		
		//List<SRegion> ret = new ArrayList<SRegion>();
		List<RegionRecord> ret = new ArrayList<RegionRecord>(); 
		while(true) {
			RegionRecord record = status.getMinImpactRegion();			
			System.out.println(record);
			
			if (record == null)
				break;
			//System.out.println(region);
			SRegion region = record.getRegionInfo().getSRegion();
			status.serialize(region);			
			ret.add(record);
		}
				
		
		Collections.reverse(ret);		
				
		return ret;
	}	
	
}

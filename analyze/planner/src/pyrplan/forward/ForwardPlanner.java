package pyrplan.forward;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

import pprof.*;
import pyrplan.FilterControl;
import pyrplan.Planner;
import pyrplan.RegionRecord;
import pyrplan.SRegionInfoFilter;
import pyrplan.backward.ProgramStatus;

public class ForwardPlanner extends Planner {

	public ForwardPlanner(SRegionInfoAnalyzer analyzer) {
		super(analyzer);
		// TODO Auto-generated constructor stub
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
			System.err.print("~");
			
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

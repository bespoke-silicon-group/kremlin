package runner;

import java.io.File;
import java.util.List;

import planner.CRegionRecord;
import planner.FilterControl;
import planner.ParameterSet;
import pprof.RegionType;
import pprof.SRegionInfoAnalyzer;
import pprof.SRegionManager;
import pprof.URegionManager;


public class KremlinNeo {
	public static void main(String args[]) {
		System.out.println("Predictor ver 0.1");		
		String timeFile = null;
		String regionFile = null;
		
		if (args.length < 1) {			
			String project = "bandwidth";			
		
			String baseDir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test";			
			ParameterSet.rawDir = baseDir + "/" + project;			
			ParameterSet.project = project;
			ParameterSet.excludeFile = project + ".exclude";			
			//timeFile = String.format("/h/g3/dhjeon/research/spaa2011/result32/%s.out.0", project);
			//regionFile = String.format("/h/g3/dhjeon/research/spaa2011/meta/%s.serial", project);		
			
		} else {
			ParameterSet.setParameter(args); 
		}
		
		String rawDir = ParameterSet.rawDir;
		String project = ParameterSet.project;
		
		String fullExcludeFile = rawDir + "/" + project + ".exclude";
		double minSP = 3.0;
		double minSpeedup = 1.001;
		//FilterControl filter = new FilterControl(ParameterSet.minSP, ParameterSet.minSpeedup, 
		//		ParameterSet.minSpeedupDOACROSS, ParameterSet.outerIncentive);
		FilterControl filter = new FilterControl(minSP, minSpeedup, 
				ParameterSet.minSpeedupDOACROSS, ParameterSet.outerIncentive);
		filter.setFilterFile(fullExcludeFile);
		filter.filterByRegionType(RegionType.BODY);
		filter.filterByRegionType(RegionType.FUNC);

		//FilterControl filter = new FilterControl(10, 10.1, 
		//				10, ParameterSet.outerIncentive);
		System.err.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";

		SRegionManager sManager = new SRegionManager(new File(sFile), true);
		URegionManager dManager = new URegionManager(sManager, new File(dFile), true);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
						
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, new OMPGrepReader());		
		profileAnalyzer.dump(rawDir + "/analysis.txt");
		//DPPlanner planner = new DPPlanner(analyzer);
		//List<CRegionRecord> plan = planner.plan(filter);		
		//planner.emitParallelRegions(rawDir + "/plan.dp");
	}
}

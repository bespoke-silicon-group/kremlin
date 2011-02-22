package runner;

import java.io.File;
import java.util.*;

import planner.CDPPlanner;
import planner.FilterControl;
import planner.ParameterSet;
import pprof.*;
import predictor.topdown.TopDownPredictor;
import pyrplan.omp.*;

public class CRegionTester {
	public static void main(String args[]) {
		System.out.println("Hello World!");
		//String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test";
		//String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test/NPB2.3-omp-C";
		
		/*String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test";
		String project = "loop";*/
		
		String dir = "/h/g3/dhjeon/research/spaa2011/spec2k";
		//String projectList[] = {"encoder"};
		String projectList[] = {"encoder"};
		//String project = "encoder";
		//String project = "bitlevel";
		//String project = "CG";
		int maxCore = 64;		
		String outDir = "/h/g3/dhjeon/research/spaa2011/graphs/hotparSerial";
		
		
		for (String project : projectList) {
			String outFile = outDir + "/" + project + ".dat";
			run(dir, project, maxCore, outFile);
		}		
		
	}

	public static void run(String dir, String project, int maxCore, String outFile) {	
		System.out.println("pyrprof ver 0.1");
		//if (args.length < 1) {
			//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/sd-vbs/disparity";
			//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/320.equake_m";
			//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/bots/alignment";
			ParameterSet.rawDir = dir;
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/330.art_m";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/320.equake_m";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/loops/dist10";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-u/is";
			//rawDir = "f:\\Work\\ep";

			//String[] splitted = rawDir.split("/");
			//project = splitted[splitted.length-1];			
			ParameterSet.project = project;
			ParameterSet.excludeFile = ParameterSet.project + ".exclude";
			
		
		
		String rawDir = ParameterSet.rawDir + "/" + ParameterSet.project;
		//String project = ParameterSet.project;
		
		String fullExcludeFile = rawDir + "/" + project + ".exclude";		
		FilterControl filter = new FilterControl(ParameterSet.minSP, ParameterSet.minSpeedup, 
				ParameterSet.minSpeedupDOACROSS, ParameterSet.outerIncentive);
		filter.filterByRegionType(RegionType.BODY);
		filter.setFilterFile(fullExcludeFile);

		//FilterControl filter = new FilterControl(10, 10.1, 
		//				10, ParameterSet.outerIncentive);
		//System.err.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";

		
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		URegionManager dManager = new URegionManager(sManager, new File(dFile), true);	
		System.err.println("URegion Built");
		//assert(false);
		CRegionManager manager = new CRegionManager(dManager);
		//assert(false);
		//manager.dump();
		System.out.println("tp = " + manager.getRoot().totalParallelism);
		assert(false);

		TopDownPredictor predictor0 = new TopDownPredictor(manager, false);		
		
		List<String> output = new ArrayList<String>();
		
		
		Set<CRegion> filter0 = new HashSet<CRegion>();
		for (int core = 1; core <=64; core*=2) {
			double predict0 = 1.0;
			double dp0 = 1.0;
			double dp1 = 1.0;
			double dp2 = 1.0;
			predict0 = predictor0.predict(core, new HashSet<CRegion>(), 0.0);		
			dp0 = getSpeedup(manager, core, 0);
			
			//for (int overhead = 1; overhead<=1024; overhead*=2) {
			double overhead = 6 + 4 * Math.log(core);
			//dp0 = getSpeedup(manager, core, (int)overhead);
			dp0 = predictor0.predict(core, new HashSet<CRegion>(), overhead);	
			dp1 = getSpeedup(manager, core, (int)overhead);
			
			
			
			
			System.out.printf("%d\t%.2f\t%.2f\t%.2f\n", core, predict0, dp0, dp1);
			output.add(String.format("%d\t%.2f\t%.2f",
					core, predict0, dp0));
			
		}
		planner.Util.writeFile(output, outFile);
	}
	
	static double getSpeedup(CRegionManager analyzer, int core, int overhead) {
		CDPPlanner dp = new CDPPlanner(analyzer, core, overhead);
		
		Set<CRegion> filter0 = new HashSet<CRegion>();
		for (CRegion each : analyzer.getCRegionSet()) {
			//if (each.getSRegion().getType() == RegionType.LOOP)
				//filter0.add(each);
		}
		double savePercent = dp.plan(filter0);		
		double speedup = 100.0 / (100.0 - savePercent);
		return speedup;
	}
}

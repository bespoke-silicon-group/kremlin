package runner;

import java.io.File;
import java.util.*;

import pprof.*;
import pyrplan.*;
import pyrplan.backward.BackwardPlanner;
import pyrplan.nested.*;

public class RunnerNest {
	public static void main(String args[]) {
		String dir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-b";
		String projectList[] = {"bt", "cg", "ep", "ft", "is", "mg", "sp"};
		String outDir = "/h/g3/dhjeon/research/spaa2011/graphs/hotpar";
		
		//String projectList[] = {"sp"};
		for (String project : projectList) {
			String outFile = outDir + "/" + project + ".dat";
			run(dir, project, 32, outFile);
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

		
		SRegionManager sManager = new SRegionManager(new File(sFile), false);		
		URegionManager dManager = new URegionManager(sManager, new File(dFile));	
		
		OMPGrepReader reader = new OMPGrepReader();		
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		profileAnalyzer.dump(rawDir + "/analysis.txt");				

		SRegionInfo root = analyzer.getRootInfo();
		//Recommender planner = new Recommender(sManager, dManager);
		//BackwardPlanner planner = new BackwardPlanner(analyzer);
		List<String> output = new ArrayList<String>();
		output.add(String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f",
				1, 1.0, 1.0, 1.0, 1.0));
		
		for (int core = 2; core<=maxCore; core*=2) {
			NestedPlanner planner0 = new NestedPlanner(analyzer, core, 0, true);
			Set<SRegionInfo> filter0 = new HashSet<SRegionInfo>();
			NestedPlanner planner1 = new NestedPlanner(analyzer, core, 0, true);
			Set<SRegionInfo> filter1 = getBodyFuncSet(analyzer);
			NestedPlanner planner2 = new NestedPlanner(analyzer, core, 0, false);
			NestedPlanner planner3 = new NestedPlanner(analyzer, core, 1000, false);
			
			
			List<RegionRecord> plan0 = planner0.plan(filter0);
			List<RegionRecord> plan1 = planner1.plan(filter1);
			List<RegionRecord> plan2 = planner2.plan(filter1);
			List<RegionRecord> plan3 = planner3.plan(filter1);
			double speedup0 = getSpeedup(plan0, root);
			double speedup1 = getSpeedup(plan1, root);
			double speedup2 = getSpeedup(plan2, root);
			double speedup3 = getSpeedup(plan3, root);
			//dumpPlan(plan1, root);
			//dumpPlan(plan3, root);
			System.out.printf("[%s]\t%d\t%.2f\t%.2f\t%.2f\t%.2f\n", 
					project, core, speedup0, speedup1, speedup2, speedup3);
			
			output.add(String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f",
					core, speedup0, speedup1, speedup2, speedup3));
					
			//assert(false);			
		}
		pyrplan.Util.writeFile(output, outFile);
	}
	
	static Set<SRegionInfo> getBodyFuncSet(SRegionInfoAnalyzer analyzer) {
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			if (each.getSRegion().getType() != RegionType.LOOP)
				ret.add(each);
		}
		return ret;
	}
	
	static double getSpeedup(List<RegionRecord> plan, SRegionInfo root) {
		if (plan.size() == 0)
			return 1.0;
		double sum = plan.get(plan.size() - 1).getExpectedExecTime();
		double speedup = root.getTotalWork() / sum;
		return speedup;
		
	}
	
	static void dumpPlan(List<RegionRecord> plan, SRegionInfo root) {
		//double sum = 0;
		double last = root.getTotalWork();			
		for (RegionRecord each : plan) {
			SRegionInfo info = each.getRegionInfo();
			double diff = last - each.getExpectedExecTime();
			double reducePercent = ((double)diff / root.getTotalWork()) * 100.0;
			last = each.getExpectedExecTime();				
			System.out.printf("[%5.2f%%] %s (%.2f %.2f) sp=%.2f cov=%.2f\n", 
					reducePercent, each, info.getMemReadRatio(), 
					info.getMemWriteRatio(), info.getSelfParallelism(), info.getCoverage());
			//sum += each.getTimeSave();
			//sum = each.getExpectedExecTime();
		}
		/*
		/*
		
		//double speedup = root.getTotalWork() / (root.getTotalWork() - sum);*/
	}
}

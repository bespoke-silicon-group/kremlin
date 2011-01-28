package runner;

import java.io.File;
import java.util.*;

import pprof.*;
import pyrplan.*;
import pyrplan.backward.BackwardPlanner;
import pyrplan.nested.*;
import predictor.topdown.*;
import pyrplan.omp.*;

public class RunnerNest {
	public static void main(String args[]) {
		String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test";
		//String dir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-b";
		//String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test/NPB2.3-omp-C";
		//String dir = "/h/g3/dhjeon/research/spaa2011/spec2k";
		//String projectList[] = {"bt", "cg", "ep", "ft", "is", "mg", "sp"};
		//String projectList[] = {"cg", "is", "sp"};
		//String projectList[] = {"twolf"};
		String projectList[] = {"loop"};
		//String projectList[] = {"BT", "CG", "FT", "IS", "EP", "LU", "SP"};
		//String projectList[] = {"164.gzip"};
		String outDir = "/h/g3/dhjeon/research/spaa2011/graphs/hotpar";
		
		//String projectList[] = {"sp"};
		for (String project : projectList) {
			String outFile = outDir + "/" + project + ".dat";
			run(dir, project, 64, outFile);
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
		
		OMPGrepReader reader = new OMPGrepReader();		
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		profileAnalyzer.dump(rawDir + "/analysis.txt");				

		SRegionInfo root = analyzer.getRootInfo();
		//Recommender planner = new Recommender(sManager, dManager);
		//BackwardPlanner planner = new BackwardPlanner(analyzer);
		List<String> output = new ArrayList<String>();
		List<String> outputR = new ArrayList<String>();
		output.add(String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f",
				1, 1.0, 1.0, 1.0, 1.0, 1.0));
		
		outputR.add(String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f",
				1, 1.0, 1.0, 1.0, 1.0, 1.0));
		
		for (int core = 2; core<=maxCore; core*=2) {
			
			
			TopDownPredictor predictor0 = new TopDownPredictor(analyzer, false);
			TopDownPredictor predictor1 = new TopDownPredictor(analyzer, true);
			
			NestedPlanner planner0 = new NestedPlanner(analyzer, core, 0, true);
			//Set<SRegionInfo> filter0 = new HashSet<SRegionInfo>();
			NestedPlanner planner1 = new NestedPlanner(analyzer, core, 0, true);
			Set<SRegionInfo> filter0 = getBodyFuncSet(analyzer);
			NestedPlanner planner2 = new NestedPlanner(analyzer, core, 0, false);
			NestedPlanner planner3 = new NestedPlanner(analyzer, core, 100000, false);
			/*			
			List<RegionRecord> plan0 = planner0.plan(filter0);
			List<RegionRecord> plan1 = planner1.plan(filter1);
			List<RegionRecord> plan2 = planner2.plan(filter1);
			List<RegionRecord> plan3 = planner3.plan(filter1);*/
			double predict0 = predictor0.predict(core, new HashSet<SRegionInfo>(), 0.0);			
			double predict1 = predictor0.predict(core, filter0, 0.0);
			double predict2 = predictor1.predict(core, filter0, 0.0);
			double predict3 = predictor1.predict(core, filter0, 1000.0);
			/*
			double speedup0 = getSpeedup(plan0, root);
			double speedup1 = getSpeedup(plan1, root);
			double speedup2 = getSpeedup(plan2, root);
			double speedup3 = getSpeedup(plan3, root);*/
			//dumpPlan(plan1, root);
			//dumpPlan(plan3, root);
			//System.out.printf("predict1 = %.2f\n", predict1);
			//assert(predict1 <= core);
			double dp0 = getSpeedup(analyzer, core, 0);
			double dp1 = getSpeedup(analyzer, core, 1000);
			double dp2 = getSpeedup(analyzer, core, 10000);
			
			
			System.out.printf("[%s]\t%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n", 
					project, core, predict0, predict2, dp0, dp1, dp2);
			
			output.add(String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f",
					core, predict0, predict2, dp0, dp1, dp2));
			
			outputR.add(String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f",
					core, getReduction(predict0), getReduction(predict2), 
					getReduction(dp0), getReduction(dp1), getReduction(dp2)));

			
			//assert(false);			
		}
		pyrplan.Util.writeFile(output, outFile);
		pyrplan.Util.writeFile(outputR, outFile + ".R");
	}
	
	static double getReduction(double speedup) {
		return 100.0 - 100.0 / speedup;
	}
	
	static double getSpeedup(SRegionInfoAnalyzer analyzer, int core, int overhead) {
		DPPlanner dp = new DPPlanner(analyzer, core, overhead);
		Set<SRegionInfo> filter0 = getBodyFuncSet(analyzer);
		dp.plan(filter0);
		double savePercent = dp.getTimeSavingPercent();
		double speedup = 100.0 / (100.0 - savePercent);
		return speedup;
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

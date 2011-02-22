package runner;

import java.io.File;
import java.util.*;

import planner.*;
import pprof.*;
import predictor.PredictUnit;
import predictor.predictors.SpeedupPredictor;
import pyrplan.backward.BackwardPlanner;
import pyrplan.omp.*;

public class RunnerOmp {
	public static void main(String args[]) {
		System.out.println("pyrprof ver 0.1");		
		
		if (args.length < 1) {			
			String project = "loop";
			String baseDir = "f:\\Work\\spatBench";
			//String baseDir = "/h/g3/dhjeon/research/pact2011/spatbench/bench-clean";			
			ParameterSet.rawDir = baseDir + "/" + project;
			//ParameterSet.rawDir = "f:\\Work\\run\\equake";
			//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-u/lu";
			//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/specOmpSerial/ammp";		
			//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/regression/bandwidth";
			ParameterSet.project = project;
			ParameterSet.excludeFile = project + ".exclude";
			
		} else {
			ParameterSet.setParameter(args);
		}
		
		String rawDir = ParameterSet.rawDir;
		String project = ParameterSet.project;
		
		String fullExcludeFile = rawDir + "/" + project + ".exclude";		
		/*
		FilterControl filter = new FilterControl(ParameterSet.minSP, ParameterSet.minSpeedup, 
				ParameterSet.minSpeedupDOACROSS, ParameterSet.outerIncentive);
		filter.setFilterFile(fullExcludeFile);
		filter.filterByRegionType(RegionType.BODY);
		filter.filterByRegionType(RegionType.FUNC);*/

		//FilterControl filter = new FilterControl(10, 10.1, 
		//				10, ParameterSet.outerIncentive);
		System.err.print("\nPlease Wait: Loading Trace Files...\n");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";

		
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		cManager.dump();
		//CDPPlanner planner = new CDPPlanner(cManager, 32, 0);		
		//double result = planner.plan(new HashSet<CRegion>());
		//assert(false);
		//System.out.printf("Result = %.2f\n", result);
		
		int spSize = 0;
		int workSize = 0;
		/*
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			if (each.getTotalParallelism() < 5.0)
				workSize++;
			if (each.getSelfParallelism() < 5.0)
				spSize++;				
		}*/
		
		/*System.out.printf("total = %d tp = %d sp = %d\n", 
				totalSize, workSize, spSize);
		System.out.println(filter);*/
	}		
}

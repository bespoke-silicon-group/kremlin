package runner;

import java.io.File;
import java.util.*;

import pprof.*;
import predictor.PredictUnit;
import predictor.predictors.SpeedupPredictor;
import pyrplan.*;
import pyrplan.backward.BackwardPlanner;
import pyrplan.omp.*;

public class RunnerOmp {
	public static void main(String args[]) {
		System.out.println("pyrprof ver 0.1");		
		
		if (args.length < 1) {			
			String project = "bandwidth";
			//String baseDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-u";
			//String baseDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-b";
			String baseDir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test";
			//String baseDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/specOmpSerial";
			//String baseDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/regression";
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
		FilterControl filter = new FilterControl(ParameterSet.minSP, ParameterSet.minSpeedup, 
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
		URegionManager dManager = new URegionManager(sManager, new File(dFile));		
		OMPGrepReader reader = new OMPGrepReader(rawDir + "/omp.txt");		
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		profileAnalyzer.dump(rawDir + "/analysis.txt");				
				
		Set<SRegionInfo> postFilterSet = filter.getPostFilterSRegionInfoSet(analyzer);
		int totalSize = analyzer.getSRegionInfoSet().size();
		int spSize = 0;
		int workSize = 0;
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			if (each.getTotalParallelism() < 5.0)
				workSize++;
			if (each.getSelfParallelism() < 5.0)
				spSize++;				
		}
		
		System.out.printf("total = %d tp = %d sp = %d\n", 
				totalSize, workSize, spSize);
		System.out.println(filter);
		//Recommender planner = new Recommender(sManager, dManager);
		//BackwardPlanner planner = new BackwardPlanner(analyzer);
		DPPlanner planner = new DPPlanner(analyzer);
		List<RegionRecord> plan = planner.plan(filter);
		planner.emitParallelRegions(rawDir + "/plan.dp");
		//List<RegionRecord> plan = planner.plan(filter);
		/*
		SpeedupPredictor predictor = new SpeedupPredictor();
		for (RegionRecord each : plan) {
			PredictUnit unit = predictor.predictSpeedup(each.getRegionInfo());
			System.out.println(unit);
		}*/
		
		
		/*
		int seq = 0;
		double sum = 0.0;
		double sumSp = 0.0;
		double sumBw = 0.0;
		double sumBwMax = 0.0;	
		
		
		Set<Integer> set = new HashSet<Integer>();
		for (RegionRecord each : plan) {
			SRegionInfo info = each.getRegionInfo();
			sum += each.getTimeSaving();
			int start = info.getSRegion().getStartLine();
			if (set.contains(start) == true)
				continue;
			set.add(start);
			int sp = (int)info.getSelfParallelism();
			if (sp > 32)
				sp = 32;
			double bwSpeedup0 = info.getAvgWork() / info.getParallelBwWork(sp);
			double bwSpeedup1 = info.getAvgWork() / info.getParallelBwWorkMax(sp);
			//double bwSpeedup1 = info.getParallelBwWork(1) / info.getParallelBwWork(sp);
			double workRatio = info.getParallelBwWork(1) / info.getAvgWork();
			sumSp += (info.getAvgWork() - info.getAvgWork() / sp) * info.getInstanceCount();
			sumBw += (info.getAvgWork() - info.getParallelBwWork(sp)) * info.getInstanceCount();
			sumBwMax += (info.getAvgWork() - info.getParallelBwWorkMax(sp)) * info.getInstanceCount();
			System.out.printf("%d %s (%.2f %.2f %.2f) work: %.2f Mem: %.2f MB Cnt: %d workRatio %.1f speedup (%.1f %.1f)\n", 
					seq++, each, 
					info.getSelfParallelism(), info.getAvgIteration(), info.getTotalParallelism(), 
					info.getCoverage(), (info.getAvgMemReadCnt() * 16 * 8) / (1024 * 1024),
					info.getInstanceCount(), workRatio, bwSpeedup0, bwSpeedup1);
					
			
		}
		
		System.out.printf("\nTotal Time Saving = %.2f, Speedup = %.2f\n", 
				sum, 100.0 / (100 - sum));
		
		
		
		double rootWork = analyzer.getRootInfo().getAvgWork();
		double spSpeedup = rootWork / (rootWork - sumSp);
		double bwSpeedup = rootWork / (rootWork - sumBw);
		double bwSpeedupMax = rootWork / (rootWork - sumBwMax);
		
		System.out.printf("\nsp Speedup = %.2f, bw Speedup = %.2f, bwMax Speedup = %.2f\n", 
				spSpeedup, bwSpeedup, bwSpeedupMax);
				*/
	}
}

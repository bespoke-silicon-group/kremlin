package runner;

import java.io.File;
import java.util.*;

import planner.*;
import pprof.*;
import predictor.*;
import predictor.predictors.*;
import trace.*;

public class PredictorRunner {
	public static void main(String args[]) {
		String projectList[] = {"bt", "cg", "ep", "ft", "is", "mg", "sp"};
		//String projectList[] = {"bt", "cg"};
		double bwList[] = {1024.0, 2048.0, 4096.0, 8192.0, 16.0 * 1024};
		double cacheList[] = {1.0, 2.0, 4.0, 8.0, 16.0};
		String baseDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-b";
		String outDir = "/h/g3/dhjeon/research/spaa2011/graphs/sensitivity";
		int maxCore = 32;
		double freq = 2660.0;
		
		for (String project : projectList) {
			for (double bw : bwList) {
				for (double cache : cacheList) {
					String outputFile = String.format("%s/%s_%d_%d.dat", outDir, project, (int)bw, (int)cache);
					run(project, baseDir, maxCore, bw, freq, cache, outputFile);
				}
			}
		}
		
		//String baseDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-u";
		//String timeFile = String.format("/h/g3/dhjeon/research/spaa2011/result32/%s.out.0", project);
		//String regionFile = String.format("/h/g3/dhjeon/research/spaa2011/meta/%s.serial", project);
		double bw = 2048.0;
				
	}
	
	public static void run(String project, String baseDir, int maxCoreNum, double bw, double freq, double cache, String file) {
		System.out.println("Predictor ver 0.1");		
		String timeFile = null;
		//String regionFile = null;		
		String regionFile = String.format("/h/g3/dhjeon/research/spaa2011/meta/%s.serial", project);
		//String baseDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/specOmpSerial/bin";
		//String baseDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/regression";
		ParameterSet.rawDir = baseDir + "/" + project;
			
		//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/regression/bandwidth";
		ParameterSet.project = project;
		ParameterSet.excludeFile = project + ".exclude";		
		
		String rawDir = ParameterSet.rawDir;
		//String project = ParameterSet.project;
		
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
		SRegionManager sManager = new SRegionManager(new File(sFile), false);		
		URegionManager dManager = new URegionManager(sManager, new File(dFile));		
		OMPGrepReader reader = new OMPGrepReader(rawDir + "/omp.txt");		
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		profileAnalyzer.dump(rawDir + "/analysis.txt");		
		
		//RegionTimeTree tree = new RegionTimeTree(timeFile, regionFile, analyzer);
		RegionTimeTree tree = new RegionTimeTree(regionFile, analyzer);
		ParallelPredictor fullPredictor = new ParallelPredictor(tree, getBwWorstPredictor(16, bw, freq));
		//fullPredictor.printRegionPrediction();
		List<String> output = new ArrayList<String>();
		output.add(String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f",
				1, 1.0, 1.0, 1.0, 1.0, 1.0));
				
		for (int maxCore=2; maxCore<=maxCoreNum; maxCore*=2) {			
			ParallelPredictor bwBestPredictor = new ParallelPredictor(tree, getBwBestPredictor(maxCore, bw, freq, cache));
			ParallelPredictor bwWorstPredictor = new ParallelPredictor(tree, getBwWorstPredictor(maxCore, bw, freq));
			ParallelPredictor amdhalPredictor = new ParallelPredictor(tree, getAmdahlPredictor(maxCore));
			ParallelPredictor spPredictor = new ParallelPredictor(tree, getSpPredictor(maxCore));
			ParallelPredictor overheadPredictor = new ParallelPredictor(tree, getOverheadPredictor(maxCore));
			
			output.add(String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f",
					maxCore,
					amdhalPredictor.getAppSpeedup(),
					spPredictor.getAppSpeedup(),
					overheadPredictor.getAppSpeedup(),
					bwBestPredictor.getAppSpeedup(),
					bwWorstPredictor.getAppSpeedup())
					);			
			
			//bwBestPredictor.printRegionPrediction();
			//bwWorstPredictor.printRegionPrediction();
			
		}
		
		planner.Util.writeFile(output, file);
		//System.out.println("\n== Coverage Speedup ==");
		//traceAnalyzer.printCoverage();
		
	}
	
	static List<ISpeedupPredictor> getSpPredictor(int maxCore) {
		List<ISpeedupPredictor> ret = new ArrayList<ISpeedupPredictor>();
		ret.add(new PredictorCore(maxCore));
		ret.add(new PredictorSp());				
		return ret;
	}

	static List<ISpeedupPredictor> getAmdahlPredictor(int maxCore) {
		List<ISpeedupPredictor> ret = new ArrayList<ISpeedupPredictor>();
		ret.add(new PredictorCore(maxCore));		
		return ret;
	}
	
	static List<ISpeedupPredictor> getBwBestPredictor(int maxCore, double bw, double freq, double cache) {
		List<ISpeedupPredictor> ret = new ArrayList<ISpeedupPredictor>();		
		ret.add(new PredictorSp());
		ret.add(new PredictorOverhead(maxCore));	
		ret.add(new PredictorBwBest(bw, freq, cache));
		return ret;		
	}
	
	static List<ISpeedupPredictor> getBwWorstPredictor(int maxCore, double bw, double freq) {
		List<ISpeedupPredictor> ret = new ArrayList<ISpeedupPredictor>();		
		ret.add(new PredictorSp());
		ret.add(new PredictorOverhead(maxCore));	
		ret.add(new PredictorBwWorst(bw, freq));
		return ret;		
	}
	
	static List<ISpeedupPredictor> getOverheadPredictor(int maxCore) {
		List<ISpeedupPredictor> ret = new ArrayList<ISpeedupPredictor>();		
		ret.add(new PredictorSp());
		ret.add(new PredictorOverhead(maxCore));
		return ret;		
	}
}

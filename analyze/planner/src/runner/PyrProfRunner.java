package runner;

import java.io.*;
import java.util.*;

import pprof.*;
import pyrplan.FilterControl;
import pyrplan.ParameterSet;
import pyrplan.RecList;
import pyrplan.SRegionInfoFilter;
import pyrplan.RecList.RecUnit;
import pyrplan.greedy.GreedyRecommender;


public class PyrProfRunner {
	static String toSRegionString(SRegion region) {
		String func = region.getFuncName();
		if (func.length() > 10)
			func = func.substring(0, 9) + "~";
		
		String module = region.getModule();
		if (module.length() > 10)
			module = module.substring(0, 9) + "~";
		
		return String.format("%15s:%15s [%4d - %4d] %s", 
				module + ".c", func, region.getStartLine(), region.getEndLine(), region.getType());
	}
	
	public static List<SRegionInfo> getRecommendedList(URegionManager dManager, SRegionInfoAnalyzer analyzer, double minSpeedup, double minSP) {
		SRegionManager sManager = dManager.getSRegionManager();
		/*
		System.out.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";

		long start = System.currentTimeMillis();
		OMPGrepReader reader = new OMPGrepReader(rawDir + "/omp.txt");
		SRegionManager sManager = new SRegionManager(new File(sFile));
		URegionManager dManager = new URegionManager(sManager, new File(dFile));
		
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		long end = System.currentTimeMillis();
		
		System.out.println("Ready Time = " + (end - start) + " ms");
		
		start = System.currentTimeMillis();*/
		GreedyRecommender rec = new GreedyRecommender(sManager, dManager);
		FilterControl filter = new FilterControl(minSP, minSpeedup, minSpeedup, minSP);
		Set<SRegion> excludeSet = createExcludeSet(sManager, analyzer, filter, null);
		RecList list = rec.recommend(0.0, excludeSet);
		//emit(analyzer, list, outFile);
		//list.dump(0.0);
		//end = System.currentTimeMillis();
		List<SRegion> ret = new ArrayList<SRegion>();
		for (int i=0; i<list.size(); i++) {
			ret.add(list.get(i).getRegion());
		}
		//emit(analyzer, list, outFile);
		return SRegionInfoFilter.toSRegionInfoList(dManager.getSRegionAnalyzer(), ret);
	}
	
	public static void main(String[] args) {
		System.out.println("pyrprof ver 0.1");
		
		//double minSpeedup = 1.001;
		//double minSP = 5.0;
		
		if (args.length < 1) {
			
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb/mg";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/330.art_m";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/320.equake_m";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/loops/dist10";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/omp-scr/qsort";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb/ft";
			ParameterSet.rawDir = "f:\\Work\\ft_dp";
			
			//String[] splitted = rawDir.split("/");
			//project = splitted[splitted.length-1];
			ParameterSet.project = "ft";
			
			
		} else {
			ParameterSet.setParameter(args);
		}
		
		String rawDir = ParameterSet.rawDir;
		String project = ParameterSet.project;
		String outFile =  rawDir + "/" + project + ".plan";
		System.err.println(rawDir + "\t" + project + "\t" + outFile);
		
		
		System.out.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";

		long start = System.currentTimeMillis();
		OMPGrepReader reader = new OMPGrepReader(rawDir + "/omp.txt");
		SRegionManager sManager = new SRegionManager(new File(sFile), false);
		URegionManager dManager = new URegionManager(sManager, new File(dFile));
		
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		long end = System.currentTimeMillis();
		
		System.out.println("Ready Time = " + (end - start) + " ms");
		
		start = System.currentTimeMillis();
		GreedyRecommender rec = new GreedyRecommender(sManager, dManager);
		FilterControl filter = new FilterControl(ParameterSet.minSP, ParameterSet.minSpeedup, 
				ParameterSet.minSpeedup, ParameterSet.minSP);
		Set<SRegion> excludeSet = createExcludeSet(sManager, analyzer, filter, null);
		RecList list = rec.recommend(0.0, excludeSet);
		emit(analyzer, list, outFile);
		//list.dump(0.0);
		end = System.currentTimeMillis();
		/* 
		FlatProfiler profiler = new FlatProfiler(list, dManager);
		profiler.dump();
		System.out.println("Ready Time = " + (end - start) + " ms");*/		
	}
	
	//public static Set<SRegionInfo> create
	
	public static Set<SRegion> createExcludeSet(SRegionManager sManager, SRegionInfoAnalyzer analyzer, FilterControl filter, String file) {
		double minDOALL = filter.getMinDOALL();
		double minDOACROSS = filter.getMinDOACROSS();
		double minSP = filter.getMinSP();
		System.out.printf("minDOALL = %.2f, minDOACROSS = %.2f, minSP = %.2f\n",
				minDOALL, minDOACROSS, minSP);
		
		Set<SRegion> loopSet = sManager.getSRegionSet(RegionType.LOOP);		 
		Set<SRegionInfo> loopInfoSet = SRegionInfoFilter.toSRegionInfoSet(analyzer, loopSet);
		Set<SRegionInfo> filteredInfoSet = SRegionInfoFilter.filterMinSpeedupAllLoop(loopInfoSet, minDOALL, minDOACROSS, minSP);
		Set<SRegionInfo> preDoAllInfoSet = SRegionInfoFilter.filterNonDoall(analyzer, filteredInfoSet, minSP);
		
		List<SRegionInfo> doAllInfoSet = SRegionInfoFilter.filterExcludedRegions(analyzer, new ArrayList<SRegionInfo>(filteredInfoSet), file);
		List<SRegion> filteredSet = SRegionInfoFilter.toSRegionList(doAllInfoSet);
		Set<SRegion> ret = sManager.getSRegionSet();
		ret.removeAll(filteredSet);	
		
		return ret;
	}
	
	public static Set<SRegion> getHighlyParallelLoopSet(Set<SRegion> loops, SRegionInfoAnalyzer analyzer) {
		Set<SRegion> ret = new HashSet<SRegion>();
		for (SRegion loop : loops) {
			SRegionInfo info = analyzer.getSRegionInfo(loop);			
			if (info == null)
				continue;
			if (loop.getType() != RegionType.LOOP)
				continue;
			
			Set<SRegion> children = info.getChildren();
			assert(children.size() == 1);
			SRegion child = children.iterator().next();
			SRegionInfo childInfo = analyzer.getSRegionInfo(child);
			
			int iter = (int)(info.getAvgWork() / childInfo.getAvgWork());	
						
			double sp = info.getSelfParallelism();
			double coverage = info.getCoverage();
			if (sp >= 6.5 && coverage >= 0.3) {
				//System.out.println("\t~" + info.getSRegion());
				ret.add(loop);
			}
		}
		return ret;
	}
	
	public static void emit(SRegionInfoAnalyzer analyzer, RecList list, String file) {
		double accSpeedup = 1.0;
		try {
			BufferedWriter output = new BufferedWriter(new FileWriter(file));
			
			Set<SRegion> exploited = analyzer.getExploitedSet();
			
			String header = String.format("[no, speedup, exploited]%10s:%15s [start - end] type %10s %10s %10s %10s\n",
					"file", "func", "speedup", "selfP", "coverage", "nsp");
			output.write(header);
			output.write("==================================================================================="
					+ "=================================\n");
			
			double sum = 0;
			for (int i=0; i<list.size(); i++) {
				RecList.RecUnit unit = list.get(i);
							
				SRegion region = unit.getRegion();
				exploited.remove(region);
				SRegionInfo info = analyzer.getSRegionInfo(region);
				RegionStatus status = info.getRegionStatus();
				boolean exploitStatus = info.isExploited();
				double speedup = 1.0 / unit.getSpeedup();
				accSpeedup = accSpeedup * speedup;
				//sum += info.getSelfReductionPercent();
				String str = String.format("[%2d, %5.2f, %5s] %s %s", 
						i, accSpeedup, exploitStatus, 
						toSRegionString(region), analyzer.formatInfoEntry(region));
				//System.out.println(str);
				output.append(str + "\n");				
			}
			double speedup = 1.0 / (1.0 - sum/100.0);
			//String upStr = String.format("Total Speedup = %.2f\n", speedup);
			//output.append(upStr);
			
			// report non-recommended regions
			int i=0;
			List<SRegionInfo> nonRecommendedList = new ArrayList<SRegionInfo>();
			for (SRegion each : exploited) {
				nonRecommendedList.add(analyzer.getSRegionInfo(each));
			}
			Collections.sort(nonRecommendedList);
			output.append("\nExploited, but non-recommended regions\n");
			for (SRegionInfo info : nonRecommendedList) {
				
				//SRegionInfo info = analyzer.getSRegionInfo(each);
				SRegion each = info.getSRegion();
				String str = String.format("[%2d, %10s]   %s %s",
						i++, info.getRegionStatus(), toSRegionString(each),
						analyzer.formatInfoEntry(each));
				output.append(str + "\n");
			}
			output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
}

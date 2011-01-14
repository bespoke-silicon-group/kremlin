package visualizer;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.*;

import visualizer.CorrelationFilter.RegionType;

public class CorrelationAnalyzer {	
	List<CorrelationEntry> list;
	
	public CorrelationAnalyzer(String file) {
		list = new ArrayList<CorrelationEntry>();
		readFile(file);
	}
	
	void readFile(String file) {
		try {
			BufferedReader reader = new BufferedReader(new FileReader(file));
			while(true) {
				String line = reader.readLine();
				if (line == null)
					break;
				if (line.length() > 1) {
					CorrelationEntry entry = new CorrelationEntry(line);
					//System.out.println(entry);
					list.add(entry);
				}
				
			}
			reader.close();
			
		} catch(Exception e) {
			e.printStackTrace();
		}
	}
	
	void process(CorrelationFilter filter) {
		for (CorrelationEntry entry : list) {
			filter.process(entry);
		}
	}
	
	public static void printLikelyRegions(CorrelationAnalyzer analyzer, RegionType type, double minWork, boolean selfP, boolean ancestor) {
		System.out.printf("{minWork %5.2f%%}\t", minWork);
		
		CorrelationFilter filter = new CorrelationFilter(type);
		filter.setMinCoverage(minWork - 0.0001);
		filter.setMinSelfP(-1);
		analyzer.process(filter);
		//System.out.println(filter + "\n");
		System.out.print(filter.parallelStatString() + "\t");
		
		for (double minSP = 2.0; minSP <= 100.0; minSP *= 2) {
			filter = new CorrelationFilter(type);
			filter.setMinCoverage(minWork - 0.0001);
			if (selfP)
				filter.setMinSelfP(minSP - 0.0001); 
			else
				filter.setMinTotalP(minSP - 0.0001);
			analyzer.process(filter);
			//System.out.println(filter + "\n");
			if (ancestor == false)
				System.out.print(filter.parallelStatString() + "\t");
			else
				System.out.print(filter.ancestorParallelStatString() + "\t");
			
		}
		System.out.println("");
	}
	
	public static void analyzeLikleyRegions(CorrelationAnalyzer analyzer, RegionType type, boolean selfP, boolean ancestor) {
		printHeader(type, selfP, ancestor);
			
		System.out.printf("%s", "minParallelism");
		for (double minSP = 1.0; minSP <= 100.0; minSP *= 2) {
			System.out.printf("%20.2f\t", minSP);
		}
		System.out.println();
		
		printLikelyRegions(analyzer, type, 0.0, selfP, ancestor);
		for (double minWork = 1.0; minWork < 100.0; minWork *= 2) {
			printLikelyRegions(analyzer, type, minWork, selfP, ancestor);
		}
	}
	
	public static void printUnlikelyRegions(CorrelationAnalyzer analyzer, RegionType type, double maxWork, boolean selfP, boolean ancestor) {
		System.out.printf("{maxWork %d%%}\t", (int)maxWork);
		for (double maxSP = 1.0; maxSP <= 100.0; maxSP *= 2) {
			CorrelationFilter filter = new CorrelationFilter(type);
			filter.setMaxCoverage(maxWork);
			if (selfP)
				filter.setMaxSelfP(maxSP);
			else
				filter.setMaxTotalP(maxSP);
			analyzer.process(filter);
			//System.out.println(filter + "\n");				
			if (ancestor == false)
				System.out.print(filter.parallelStatString() + "\t");
			else
				System.out.print(filter.ancestorParallelStatString() + "\t");
		}
		
		CorrelationFilter filter = new CorrelationFilter(type);
		filter.setMaxCoverage(maxWork);
		filter.setMaxSelfP(-1);
		analyzer.process(filter);
		//System.out.println(filter + "\n");
		System.out.print(filter.parallelStatString() + "\t");
		System.out.println("");
	}
	
	public static void analyzeUnlikleyRegions(CorrelationAnalyzer analyzer, RegionType type, boolean selfP, boolean ancestor) {

		printHeader(type, selfP, ancestor);
		System.out.printf("%s", "maxParallelism");
		for (double maxSP = 1.0; maxSP <= 100.0; maxSP *= 2) {
			System.out.printf("%22.2f", maxSP);
		}
		System.out.printf("%20s\t", "unlimited");
		System.out.println();
		for (double maxWork = 1; maxWork < 200.0; maxWork *= 2) {
			printUnlikelyRegions(analyzer, type, maxWork, selfP, ancestor);
		}
		
	}
	
	public static void printBucketedRegions(CorrelationAnalyzer analyzer, RegionType type, double minWork, double maxWork, boolean selfP, boolean ancestor) {
		System.out.printf("Work {%d%%, %d%%}\t", (int)minWork, (int)maxWork);
		
		double minSP = -1.0;
		double maxSP = -1.0;
		for (maxSP = 2.0; maxSP <= 100.0; maxSP *= 2) {
			CorrelationFilter filter = new CorrelationFilter(type);
			filter.setMinCoverage(minWork);
			filter.setMaxCoverage(maxWork);
			
			if (selfP) {
				filter.setMinSelfP(minSP);
				filter.setMaxSelfP(maxSP);
			}
			else {
				filter.setMinTotalP(minSP);
				filter.setMaxTotalP(maxSP);
			}
			analyzer.process(filter);
			//System.out.println(filter + "\n");				
			if (ancestor == false)
				System.out.print(filter.parallelStatString() + "\t");
			else
				System.out.print(filter.ancestorParallelStatString() + "\t");
			
			minSP = maxSP;
		}
		
		CorrelationFilter filter = new CorrelationFilter(type);
		filter.setMinCoverage(minWork);
		filter.setMaxCoverage(maxWork);
		maxSP = -1.0;
		if (selfP) {
			filter.setMinSelfP(minSP);
			filter.setMaxSelfP(maxSP);
		}
		else {
			filter.setMinTotalP(minSP);
			filter.setMaxTotalP(maxSP);
		}
		analyzer.process(filter);
		//System.out.println(filter + "\n");
		//System.out.print(filter.parallelStatString() + "\t");
		
		if (ancestor == false)
			System.out.print(filter.parallelStatString() + "\t");
		else
			System.out.print(filter.ancestorParallelStatString() + "\t");
		System.out.println("");
	}
	
	static void printHeader(RegionType type, boolean selfP, boolean ancestor) {
		System.out.println("Target Region Types: " + type);	
		System.out.print("Parallelism Type: ");
		if (selfP)
			System.out.println("Self Parallelism");
		else
			System.out.println("Total Parallelism");
		if (ancestor)
			System.out.println("Include Ancestor Parallelized Regions");
	}
	
	public static void analyzeBucketRegions(CorrelationAnalyzer analyzer, RegionType type, boolean selfP, boolean ancestor) {
		printHeader(type, selfP, ancestor);
		
		System.out.printf("%s", "maxParallelism");
		for (double maxSP = 1.0; maxSP <= 100.0; maxSP *= 2) {
			System.out.printf("%22.2f", maxSP);
		}
		System.out.printf("%20s\t", "unlimited");
		System.out.println();
		double minWork = -1.0;
		for (double maxWork = 1; maxWork < 200.0; maxWork *= 2) {
			printBucketedRegions(analyzer, type, minWork, maxWork, selfP, ancestor);
			minWork = maxWork;
		}
		
	}
	
	public static void main(String args[]) {
		String root = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/chartGen/merged.dat";
		CorrelationAnalyzer analyzer = new CorrelationAnalyzer(root);
		
		analyzeBucketRegions(analyzer, RegionType.REGION_LOOP, true, false);		
		System.out.println("");
		System.out.println("");		
		analyzeBucketRegions(analyzer, RegionType.REGION_FUNC, false, false);
		
		/*analyzeBucketRegions(analyzer, RegionType.REGION_LOOP, false, true);
		System.out.println("");
		System.out.println("");*/
		
		/*
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_LOOP, true, false);		
		System.out.println("");
		System.out.println("");		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_LOOP, true, true);
		System.out.println("");
		System.out.println("");
		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_LOOP, false, false);		
		System.out.println("");
		System.out.println("");		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_LOOP, false, true);
		System.out.println("");
		System.out.println("");*/
		
		
		/*
		CorrelationFilter filter = new CorrelationFilter(RegionType.REGION_ALL);
		analyzer.process(filter);
		System.out.println(filter);
		
		CorrelationFilter filter2 = new CorrelationFilter(RegionType.REGION_ALL);
		filter2.setMinSelfP(10);
		filter2.setMinCoverage(10);
		analyzer.process(filter2);
		System.out.println(filter2);
		
		CorrelationFilter filter3 = new CorrelationFilter(RegionType.REGION_ALL);
		filter3.setMaxSelfP(2.0);
		//filter3.minCoverage = 0.1;
		analyzer.process(filter3);
		System.out.println(filter3);*/
		
		/*
		analyzeLikleyRegions(analyzer, RegionType.REGION_LOOP, true);		
		System.out.println("");
		System.out.println("");		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_LOOP, true);
		System.out.println("");
		System.out.println("");
		
		analyzeLikleyRegions(analyzer, RegionType.REGION_FUNC, true);		
		System.out.println("");
		System.out.println("");		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_FUNC, true);
		System.out.println("");
		System.out.println("");
		
		analyzeLikleyRegions(analyzer, RegionType.REGION_ALL, true);		
		System.out.println("");
		System.out.println("");		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_ALL, true);
		System.out.println("");
		System.out.println("");
		
		
		// total Parallelism
		
		analyzeLikleyRegions(analyzer, RegionType.REGION_LOOP, false);		
		System.out.println("");
		System.out.println("");		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_LOOP, false);
		System.out.println("");
		System.out.println("");
		
		analyzeLikleyRegions(analyzer, RegionType.REGION_FUNC, false);		
		System.out.println("");
		System.out.println("");		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_FUNC, false);
		System.out.println("");
		System.out.println("");
		
		analyzeLikleyRegions(analyzer, RegionType.REGION_ALL, false);		
		System.out.println("");
		System.out.println("");		
		analyzeUnlikleyRegions(analyzer, RegionType.REGION_ALL, false);
		System.out.println("");
		System.out.println("");*/
	}
	
}

package runner;

import java.io.File;
import java.util.*;

import planner.*;
import pprof.*;

public class KremlinOMP {
	/**
	 * @param args
	 */
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		String baseDir = null;
		if (args.length < 1) {			
			baseDir = "f:\\Work\\spatBench\\loop";
			System.out.println("Usage: java KremlinOMP <dir>\n");
		} else {
			baseDir = args[0];
		}
					
		ParameterSet.rawDir = baseDir;		
		ParameterSet.project = baseDir;		
		String rawDir = ParameterSet.rawDir;		
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";

		
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		CDPPlanner planner = new CDPPlanner(cManager, 32, 0);		
		List<CRegionRecord> result = planner.plan(new HashSet<CRegion>());
		List<CRegion> list = new ArrayList<CRegion>();
		for (CRegionRecord each : result)
			list.add(each.getCRegion());
		
		CRegionPrinter printer = new CRegionPrinter(cManager);
		printer.printRegionList(list);		
		//assert(false);
		//System.out.printf("Result = %.2f\n", result);
		/*
		Map<CRegion, Double> map = new HashMap<CRegion, Double>();
		for (CRegion region : cManager.getCRegionSet()) {
			map.put(region, cManager.getTimeReduction(region));			
		}
		map = getSortedCRegionMap(map);
		System.out.println("Kremlin Profiler Ver 0.1\n");
		printMap(new CRegionPrinter(cManager), map);*/
	}
	
	static void printMap(CRegionPrinter printer, Map<CRegion, Double> map) {
		int index = 0;
		for (CRegion region : map.keySet()) {
			System.out.printf("[%3d] %s\n%s\n", 
					index++, printer.getStatString(region), printer.getContextString(region));
		}
	}
	
	static Map<CRegion, Double> getSortedCRegionMap(Map<CRegion, Double> map) {
		Map<CRegion, Double> ret = new LinkedHashMap<CRegion, Double>();
		List<CRegion> mapKeys = new ArrayList<CRegion>(map.keySet());
		List<Double> mapValues = new ArrayList<Double>(map.values());
		TreeSet<Double> sortedSet = new TreeSet<Double>(mapValues);
		Object[] sortedArray = sortedSet.toArray();
		int size = sortedArray.length;
		
		for (int i=size-1; i>=0; i--) {
			ret.put(mapKeys.get(mapValues.indexOf(sortedArray[i])), (Double)sortedArray[i]);
		}
		return ret;
	}
}

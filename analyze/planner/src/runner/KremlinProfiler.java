package runner;

import java.io.File;

import pprof.*;
import pyrplan.ParameterSet;
import java.util.*;

public class KremlinProfiler {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		String baseDir = null;
		if (args.length < 1) {			
			//baseDir = "f:\\Work\\spatBench\\loop";
			System.out.println("Usage: java KremlinProfiler <dir>\n");
		} else {
			baseDir = args[0];
		}
		
		
		//String baseDir = "/h/g3/dhjeon/research/pact2011/spatbench/bench-clean";			
		ParameterSet.rawDir = baseDir;
		//ParameterSet.rawDir = "f:\\Work\\run\\equake";
		//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-u/lu";
		//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/specOmpSerial/ammp";		
		//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/regression/bandwidth";
		ParameterSet.project = baseDir;		
		String rawDir = ParameterSet.rawDir;		
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";

		
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		Map<CRegion, Double> map = new HashMap<CRegion, Double>();
		for (CRegion region : cManager.getCRegionSet()) {
			map.put(region, cManager.getTimeReduction(region));			
		}
		map = getSortedCRegionMap(map);
		System.out.println("Kremlin Profiler Ver 0.1\n");
		printMap(new CRegionPrinter(cManager), map);
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

import java.io.File;
import planner.ParameterSet;
import pprof.*;
import java.util.*;

public class KremlinProfiler {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		String baseDir = null;
		if (args.length < 1) {			
			baseDir = ".";
			
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
		List<CRegion> list = new ArrayList<CRegion>();
		for (CRegion region : cManager.getCRegionSet()) {
			double myReduction = cManager.getTimeReduction(region);
			map.put(region, myReduction);
			int index = 0;
			for (index=0; index<list.size(); index++) {
				CRegion each = list.get(index);
				double yourReduction = map.get(each);
				if (myReduction > yourReduction) {					
					break;
				}
			}		
			list.add(index, region);
		}	
		
		System.out.println("Kremlin Profiler Ver 0.1\n");
		CRegionPrinter printer = new CRegionPrinter(cManager);
		printer.printRegionList(list);		
	}	
}

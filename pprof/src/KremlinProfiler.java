import java.io.File;
import planner.ParameterSet;
import pprof.*;
import java.util.*;

public class KremlinProfiler {

	/**
	 * @param args
	 */
	//public static void main(String[] args) {
	public static void run(ArgDB db) {
		// TODO Auto-generated method stub
		String baseDir = null;		
		
		//String baseDir = "/h/g3/dhjeon/research/pact2011/spatbench/bench-clean";			
		ParameterSet.rawDir = db.getPath();		
		ParameterSet.project = baseDir;		
		String rawDir = ParameterSet.rawDir;		
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";

		
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		Map<CRegion, Double> map = new HashMap<CRegion, Double>();
		List<CRegion> list = new ArrayList<CRegion>();
		//System.out.println("Size = " + cManager.getCRegionSet().size());
		class CRegionComparator implements Comparator {
			CRegionManager manager;
			CRegionComparator(CRegionManager manager) {
				this.manager = manager;
			}
			@Override
			public int compare(Object o1, Object o2) {
				// TODO Auto-generated method stub
				CRegion c1 = (CRegion)o1;
				CRegion c2 = (CRegion)o2;
				double diff = (manager.getTimeReduction(c1) - manager.getTimeReduction(c2));
				if (diff == 0.0)
					return 0;
				return (diff > 0) ? -1 : 1;
			}
			
		}

		if (db.thresholdReduction > 0.0) {
			list = new ArrayList<CRegion>(cManager.getCRegionSet(db.thresholdReduction));
		} else {
			list = new ArrayList<CRegion>(cManager.getCRegionSet());
		}
		
		Collections.sort(list, new CRegionComparator(cManager));
		/*
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
		}*/	
		
		System.out.println("Kremlin Profiler Ver 0.1\n");
		CRegionPrinter printer = new CRegionPrinter(cManager);
		printer.printRegionList(list);
		cManager.printStatistics();
	}
	
	
}
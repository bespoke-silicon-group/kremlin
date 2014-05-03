import java.io.File;
import planner.ParameterSet;
import kremlin.*;

import java.util.*;

public class KremlinQuery {

	/**
	 * @param args
	 */
	//public static void main(String[] args) {
	public static void run() {
		// TODO Auto-generated method stub
		String baseDir = null;		
		
		//String baseDir = "/h/g3/dhjeon/research/pact2011/spatbench/bench-clean";			
		ParameterSet.rawDir = KremlinConfig.getPath();		
		ParameterSet.project = baseDir;		
		String rawDir = ParameterSet.rawDir;		
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";

		
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		KremlinConfig kc = KremlinConfig.getInstance();
		SRegion sr = sManager.getSRegion(kc.getSRegionQueryId());
		Set<CRegion> set = cManager.getCRegionSet(sr);

		KremlinPrinter printer = KremlinPrinter.configure(cManager);
		System.out.println("Kremlin Querier\n");
		if (set != null)
			printRegionSet(printer, set);
		else
			System.out.println("No region found with id = " 
								+ kc.getSRegionQueryId());
		cManager.printStatistics();
	}
	
	public static void printRegionSet(KremlinPrinter printer, Set<CRegion> cRegions) {
		int index = 0;
		long total = 0;
		KremlinConfig db = KremlinConfig.getInstance();
		for (CRegion region : cRegions) {
			String entry = null;
			if (db.isVerbose()) {
				entry = String.format("[%3d] %s\n\n%s", 
						index++, printer.getVerboseString(region), printer.getContextString(region));
				
			} else {
				entry = String.format("[%3d] %s\n\n%s", 
						index++, printer.getSummaryString(region), printer.getContextString(region));
			}
			
			System.out.printf("%s\n\n", entry);
			total += region.getInstanceCount();
		}
		System.out.printf("Total Region Count = %d\n", total);
	}
	
	
}

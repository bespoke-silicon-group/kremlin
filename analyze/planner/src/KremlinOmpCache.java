import java.io.File;
import java.util.HashSet;
import java.util.Set;

import planner.*;
import pprof.*;



public class KremlinOmpCache {
	public static void main(String[] args) {		
		String baseDir = ".";
		//baseDir="f:\\Work\\pact2011";
		//baseDir="f:\\Work\\pact2011\\cg";
		int numCore = 32;
		int overhead = 1024;
		 
		if (args.length < 1) {			
			//baseDir = ".";
			//System.out.println("Usage: java KremlinOMP <dir> <core>\n");
		} else {
			baseDir = args[0];			
		}
		
		if (args.length > 2) {
			numCore = Integer.parseInt(args[1]);
			overhead = Integer.parseInt(args[2]);
		}
					
		ParameterSet.rawDir = baseDir;		
		ParameterSet.project = baseDir;		
		String rawDir = ParameterSet.rawDir;		
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";
		String cacheFile = rawDir + "/cache.txt";

		SRegionManager sManager = new SRegionManager(new File(sFile), true);
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		CacheManager manager = new CacheManager(cManager, cacheFile);
		Set<CRegion> excludeSet = getNonLoopSet(cManager);
		Target target = new Target(numCore, overhead);
		CacheAwarePlanner planner = new CacheAwarePlanner(manager, target);
		Plan plan = planner.plan(excludeSet);
		PlanPrinter.print(cManager, plan);
	}	
	
	public static Set<CRegion> getNonLoopSet(CRegionManager manager) {
		Set<CRegion> ret = new HashSet<CRegion>();
		for (CRegion each : manager.getCRegionSet()) {
			if (each.getSRegion().getType() != RegionType.LOOP)
				ret.add(each);			
		}		
		return ret;
	}
}

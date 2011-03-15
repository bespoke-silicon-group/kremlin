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
		int llCacheMaxScaleFactor = 8;
		 
		if (args.length < 1) {			
			//baseDir = ".";
			//System.out.println("Usage: java KremlinOMP <dir> <core>\n");
		} else {
			baseDir = args[0];			
		}
		
		if (args.length > 2) {
			numCore = Integer.parseInt(args[1]);
			overhead = Integer.parseInt(args[2]);
			llCacheMaxScaleFactor = Integer.parseInt(args[3]);
		}
					
		ParameterSet.rawDir = baseDir;		
		ParameterSet.project = baseDir;		
		String rawDir = ParameterSet.rawDir;		
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";
		String cacheFile = rawDir + "/cache.txt";

		SRegionManager sManager = new SRegionManager(new File(sFile), true);
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		CacheManager manager = new CacheManager(cManager, cacheFile, llCacheMaxScaleFactor);
		Set<CRegion> excludeSet = getNonLoopSet(cManager);
		Target target = new Target(numCore, overhead);
		CacheAwarePlanner planner = new CacheAwarePlanner(manager, target);
		Plan plan = planner.plan(excludeSet);
		PlanPrinter.print(cManager, plan);
		
		double reduction0 = 0.0;
		double reduction1 = 0.0;
		for (CRegionRecord record : plan.getCRegionList()) {
			CRegion region = record.getCRegion();
			int core = record.getCoreCount();
			reduction0 += manager.getCacheServiceTimeReduction(region, 0, core);
			reduction1 += manager.getCacheServiceTimeReduction(region, 1, core);
		}
		double totalCacheTime0 = manager.getCacheServiceTime(cManager.getRoot(), 0, 1);
		double totalCacheTime1 = manager.getCacheServiceTime(cManager.getRoot(), 1, 1);
		double cacheServiceTime0 = totalCacheTime0 - reduction0;
		double cacheServiceTime1 = totalCacheTime1 - reduction1;
		double cache0Percent = cacheServiceTime0 / plan.getParallelTime() * 100.0;
		double cache1Percent = cacheServiceTime1 / plan.getParallelTime() * 100.0;
		double computePercent = 100.0 - cache0Percent - cache1Percent;
		double accumCache0Percent = computePercent + cache0Percent;
		double accumCache1Percent = accumCache0Percent + cache1Percent;
		System.out.printf("Total Time = %.2f\n", plan.getParallelTime());
		System.out.printf("CacheServiceTime0 = %.2f\n", cacheServiceTime0);
		System.out.printf("CacheServiceTime1 = %.2f\n", cacheServiceTime1);
		
		System.out.printf("Percentage:\t%d\t%.2f\t%.2f\t%.2f\n", numCore, computePercent, accumCache0Percent, 100.0);				
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

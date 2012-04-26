

import java.io.File;
import java.util.*;

import planner.*;
import pprof.*;



public class KremlinGPU {
	/**
	 * @param args
	 */
	//public static void main(String[] args) throws Exception {
	public static void run() {
		String baseDir = ".";		
		int numCore = 32;
				
		numCore = KremlinConfig.getCoreCount();
		baseDir = KremlinConfig.getPath();
					
		ParameterSet.rawDir = baseDir;		
		ParameterSet.project = baseDir;		
		String rawDir = ParameterSet.rawDir;		
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";

				
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		Set<CRegion> excludeSet = getNonLoopSet(cManager);
		Target target = new Target(numCore, KremlinConfig.getOverhead());
		CDPPlanner planner = new CDPPlanner(cManager, target);
		//BWPlannerWorst planner = new BWPlannerWorst(cManager, target);
		//BWPlannerBest planner = new BWPlannerBest(cManager, target);
		Plan plan = planner.plan(excludeSet);		
		PlanPrinter.print(cManager, plan, KremlinConfig.getThresholdReduction());
		
		if (KremlinConfig.showRegionCount())
			cManager.printStatistics();
	}	
	
	public static Set<CRegion> getNonLoopSet(CRegionManager manager) {
		Set<CRegion> ret = new HashSet<CRegion>();
		for (CRegion each : manager.getCRegionSet()) {
			if (each.getSRegion().getType() != RegionType.LOOP)
				ret.add(each);
			
			else if (!each.getParallelBit())
				ret.add(each);
		}		
		return ret;
	}
}

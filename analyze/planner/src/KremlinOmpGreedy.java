import java.io.File;
import java.util.HashSet;
import java.util.Set;

import planner.*;
import pprof.CRegion;
import pprof.CRegionManager;
import pprof.RegionType;
import pprof.SRegionManager;


public class KremlinOmpGreedy {
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		String baseDir = ".";
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

				
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		Set<CRegion> excludeSet = getNonLoopSet(cManager);
		Target target = new Target(numCore, overhead);
		GreedyPlanner planner = new GreedyPlanner(cManager, target);
		//BWPlannerWorst planner = new BWPlannerWorst(cManager, target);
		//BWPlannerBest planner = new BWPlannerBest(cManager, target);
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

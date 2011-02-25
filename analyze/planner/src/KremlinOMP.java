

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
		String baseDir = ".";
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
		Target target = new Target(numCore, overhead);
		CDPPlanner planner = new CDPPlanner(cManager, target);		
		Plan plan = planner.plan(new HashSet<CRegion>());		
		PlanPrinter.print(cManager, plan);		
	}	
}

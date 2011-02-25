import java.io.File;
import java.util.*;
import planner.*;
import pprof.*;


public class KremlinRaw {
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		String baseDir = ".";
		int numCore = 2;
		int overhead = (int)(2 * Math.log(numCore));
		 
		if (args.length < 1) {			
			//baseDir = "f:\\Work\\spatBench\\09.sha";			
			System.out.println("Usage: java KremlinRAW <dir> <core>\n");
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
		CDPPlanner planner = new CDPPlanner(cManager, new Target(numCore, overhead));
		
		Set<CRegion> excludeSet = getNonLeafLoopSet(cManager);		
		Plan plan = planner.plan(excludeSet);
		
		PlanPrinter.print(cManager, plan);		
	}
	
	public static Set<CRegion> getNonLeafLoopSet(CRegionManager manager) {
		Set<CRegion> ret = new HashSet<CRegion>();
		for (CRegion each : manager.getCRegionSet()) {
			if (!isLeafLoop(each))
				ret.add(each);			
		}		
		return ret;
	}
	
	// should not contain any function or loop except loop bodies
	static boolean isLeafLoop(CRegion region) {
		SRegion sregion = region.getSRegion();
		//if (sregion.getType() != RegionType.LOOP)
		//	return false;
		
		List<CRegion> ready = new ArrayList<CRegion>(region.getChildrenSet());		
		while (!ready.isEmpty()) {
			CRegion current = ready.remove(0);
			SRegion sCurrent = current.getSRegion();
			RegionType type = sCurrent.getType();
			if (type == RegionType.LOOP || type == RegionType.FUNC) {
				return false;
			}
			ready.addAll(current.getChildrenSet());
		}
		//System.out.println(region);
		return true;		
	}
}

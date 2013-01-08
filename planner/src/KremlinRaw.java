import java.io.File;
import java.util.*;
import planner.*;
import kremlin.*;


public class KremlinRaw {
	public static void run(KremlinConfig db) {
		// TODO Auto-generated method stub
		String baseDir = db.getPath();
		int numCore = db.getCoreCount();
		int overhead = (int)(2 * Math.log(numCore));		 
		
					
		ParameterSet.rawDir = baseDir;		
		ParameterSet.project = baseDir;		
		String rawDir = ParameterSet.rawDir;		
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/kremlin.bin";

		
		SRegionManager sManager = new SRegionManager(new File(sFile), true);		
		CRegionManager cManager = new CRegionManager(sManager, dFile);
		NoNestPlanner planner = new NoNestPlanner(cManager, new Target(numCore, overhead));
		
		Set<CRegion> excludeSet = getNonLeafLoopSet(cManager);		
		Plan plan = planner.plan(excludeSet);
		
		PlanPrinter.print(cManager, plan, db.getThresholdReduction());		
	}
	
	public static Set<CRegion> getNonLeafLoopSet(CRegionManager manager) {
		Set<CRegion> ret = new HashSet<CRegion>();
		for (CRegion each : manager.getCRegionSet()) {
			if (!isExploitable(each))
				ret.add(each);			
		}		
		return ret;
	}
	
	// should not contain any function or loop except loop bodies
	static boolean isExploitable(CRegion region) {
		SRegion sregion = region.getSRegion();
		//if (sregion.getType() != RegionType.LOOP)
		//	return false;
		if (sregion.getType() == RegionType.LOOP) {
			return false;
		}
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

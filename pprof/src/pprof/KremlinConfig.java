package pprof;
import joptsimple.*;
import planner.*;


public class KremlinConfig {
	static KremlinConfig instance = null;
	
	public static KremlinConfig configure(OptionSet set) {
		if (instance == null) {
			instance = new KremlinConfig(set);
		}
		return instance;
	}
	
	public static KremlinConfig getInstance() {
		assert(instance != null);
		return instance;
	}	
		
	String path =".";
	//String path = "g:\\work\\ktest\\fft";
	int numCore;	
	int overhead;
	boolean showRegionCount = true;
	boolean verbose = false;
	double thresholdReduction = 5.0;	
	PlannerType type;
	
	
	
	private KremlinConfig(OptionSet set) {
		this.numCore = Integer.valueOf((String)set.valueOf("cores"));
		this.overhead = Integer.valueOf((String)set.valueOf("overhead"));
		//this.planner = (String)set.valueOf("planner");
		//this.planner = "gPU";
		String plannerString = (String)set.valueOf("planner");
		this.type = PlannerType.fromString(plannerString);
		if (this.type == null)
			this.type = PlannerType.GPU;
		
		this.thresholdReduction = Float.valueOf((String)set.valueOf("min-time-reduction"));
		//this.showRegionCount = set.has("region-count");
		if (set.has("verbose"))
			this.verbose = true;
		
	}	
	
	public static String getPath() {
		return instance.path;
	}
	
	public static int getCoreCount() {
		return instance.numCore;
	}
	
	public static int getOverhead() {
		return instance.overhead;
	}
	
	public static boolean isVerbose() {
		return instance.verbose;
	}
	
	public static double getThresholdReduction() {
		return instance.thresholdReduction;
	}
	
	public static boolean showRegionCount() {
		return instance.showRegionCount;
	}
	
	public static PlannerType getPlanner() {
		return instance.type;
	}
	
	
}

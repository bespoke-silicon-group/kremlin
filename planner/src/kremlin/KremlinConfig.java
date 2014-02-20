package kremlin;
import joptsimple.*;
import planner.*;


/*
 * Class that represents configuration options when running the kremlin
 * planner tool.
 */
public class KremlinConfig {
	static KremlinConfig instance = null;

	String path =".";
	int numCore;
	int overhead;
	boolean showRegionCount = true;
	boolean verbose = false;
	double thresholdReduction = 5.0;	
	PlannerType type;
	
	/*
	 * Set the single instance of our config options using the options given
	 * as input.
	 */
	public static KremlinConfig configure(OptionSet set) {
		if (instance == null) {
			instance = new KremlinConfig(set);
		}
		return instance;
	}
	
	/*
	 * Private constructor.
	 * Use the configure method to configure rather than directly calling
	 * this.
	 */
	private KremlinConfig(OptionSet set) {
		this.numCore = Integer.valueOf((String)set.valueOf("cores"));
		this.overhead = Integer.valueOf((String)set.valueOf("overhead"));
		String plannerString = (String)set.valueOf("planner");
		this.type = PlannerType.fromString(plannerString);
		if (this.type == null) this.type = PlannerType.GPU;
		
		this.thresholdReduction = Float.valueOf((String)set.valueOf("min-time-reduction"));
		if (set.has("verbose")) this.verbose = true;
		
	}	
	
	/* Getters */
	public static KremlinConfig getInstance() {
		assert(instance != null);
		return instance;
	}	
	public static String getPath() { return instance.path; }
	public static int getCoreCount() { return instance.numCore; }
	public static int getOverhead() { return instance.overhead; }
	public static boolean isVerbose() { return instance.verbose; }
	public static double getThresholdReduction() {
		return instance.thresholdReduction;
	}
	public static boolean showRegionCount() { return instance.showRegionCount; }
	public static PlannerType getPlanner() { return instance.type; }
}

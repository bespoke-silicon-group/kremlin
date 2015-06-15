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
	long sregionQueryId = 0;
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
		if (this.type == null) this.type = PlannerType.Profiler;

		// @TRICKY: Java doesn't like to read in Longs that are 64 bits 
		// and have a 1 in the most significant bit.
		String s = (String)set.valueOf("sregion");
		if(s.length() == 16 && s.charAt(0) > '7') {
			char fudge;
			switch (s.charAt(0)) {
				case '8': fudge = '0'; break;
				case '9': fudge = '1'; break;
				case 'a': fudge = '2'; break;
				case 'b': fudge = '3'; break;
				case 'c': fudge = '4'; break;
				case 'd': fudge = '5'; break;
				case 'e': fudge = '6'; break;
				case 'f': fudge = '7'; break;
				default: fudge = 'x'; break;
			}
			this.sregionQueryId = Long.valueOf(fudge + s.substring(1), 16);
			this.sregionQueryId |= (1L << 63);
		}
		else {
			this.sregionQueryId = Long.valueOf(s, 16);
		}
		
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
	public static Long getSRegionQueryId() { return instance.sregionQueryId; }
}

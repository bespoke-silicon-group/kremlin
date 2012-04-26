package pprof;
import joptsimple.OptionParser;
import joptsimple.OptionSet;


public class ArgDB {
	static ArgDB instance = null;
	
	public static ArgDB getInstance(OptionSet set) {
		if (instance == null) {
			instance = new ArgDB(set);
		}
		return instance;
	}
	
	public static ArgDB getInstance() {
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
	String planner = "none";
	
	/*
	ArgDB(String[] args) throws Exception {
		OptionParser parser = new OptionParser();
		parser.accepts("cores", "number of cores").withOptionalArg().describedAs("int").defaultsTo("4");
		
		OptionSet options = parser.parse("--cores=3");
		//System.out.println("core = " + options.valueOf("core"));
		//parser.printHelpOn(System.out);
		
	}*/
	
	private ArgDB(OptionSet set) {
		this.numCore = Integer.valueOf((String)set.valueOf("cores"));
		this.overhead = Integer.valueOf((String)set.valueOf("overhead"));
		this.planner = (String)set.valueOf("planner");
		this.thresholdReduction = Float.valueOf((String)set.valueOf("min-time-reduction"));
		//this.showRegionCount = set.has("region-count");
		if (set.has("verbose"))
			this.verbose = true;
		
	}
	
	public String getPlanner() {
		return instance.planner;
	}
	
	public String getPath() {
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
	
	
}

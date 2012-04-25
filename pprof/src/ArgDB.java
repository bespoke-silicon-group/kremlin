import joptsimple.OptionParser;
import joptsimple.OptionSet;


public class ArgDB {
	String path =".";	
	int numCore;	
	int overhead;
	boolean showRegionCount = true;
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
	
	ArgDB(OptionSet set) {
		this.numCore = Integer.valueOf((String)set.valueOf("cores"));
		this.overhead = Integer.valueOf((String)set.valueOf("overhead"));
		this.planner = (String)set.valueOf("planner");
		this.thresholdReduction = Float.valueOf((String)set.valueOf("min-time-reduction"));
		//this.showRegionCount = set.has("region-count");
		
	}
	
	String getPlanner() {
		return planner;
	}
	
	String getPath() {
		return path;
	}
	
	int getCoreCount() {
		return numCore;
	}
	
	int getOverhead() {
		return overhead;
	}
	
	
}

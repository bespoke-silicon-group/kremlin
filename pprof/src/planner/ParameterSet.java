package planner;

public class ParameterSet {
	public static double minSpeedup = 1.001;
	public static double minSpeedupDOACROSS = 1.03;
	public static double minSP = 5.0;
	public static double outerIncentive = 1.1;
	public static String rawDir = null;
	public static String project = null;
	public static String excludeFile = null;
	
	// minWorkChunk:
	// the min amount of work that can result in profitable parallelization
	// the lower minWorkChunk implies the target supports 
	// finer grained parallelization
	private final static double minWorkChunkIdeal = 1.0;
	public static double minWorkChunk = minWorkChunkIdeal;
	
	// bwThreshold: number of cores that can be handled by the chip
	// when those cores continuously issue load instructions
	// higher value means the chip supports higher memory bandwidth
	private final static double bwThresholdIdeal = 10000.0;
	public static double bwThreshold = bwThresholdIdeal;
	
	public static void setParameter(String[] args) {
		ParameterSet.rawDir = args[0];
		ParameterSet.project = args[1];
		if (args.length >= 3) {
			ParameterSet.minSpeedup = Double.parseDouble(args[2]);
		}
		if (args.length >= 4) {
			ParameterSet.minSpeedupDOACROSS = Double.parseDouble(args[3]);
		}
		if (args.length >= 5) {
			ParameterSet.minSP = Double.parseDouble(args[4]);
		}
		if (args.length >= 6) {
			ParameterSet.outerIncentive = Double.parseDouble(args[5]);
		}
		
		if (args.length >= 7) {
			ParameterSet.minWorkChunk = Double.parseDouble(args[6]);
		}
		
		if (args.length >= 8) {
			ParameterSet.bwThreshold = Double.parseDouble(args[7]);
		}		
	}
	
}

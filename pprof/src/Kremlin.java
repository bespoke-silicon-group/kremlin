import pprof.ArgDB;
import joptsimple.OptionParser;
import joptsimple.OptionSet;


public class Kremlin {
	static OptionParser configureParser() {
		OptionParser parser = new OptionParser();
		parser.accepts("help");
		parser.accepts("cores", "number of cores").withOptionalArg().describedAs("int").defaultsTo("4");
		parser.accepts("min-time-reduction").withOptionalArg().describedAs("float").defaultsTo("0.0");
		parser.accepts("planner").withOptionalArg().defaultsTo("gpu");
		parser.accepts("overhead").withOptionalArg().defaultsTo("0");
		//OptionSet options = parser.parse("--cores=3");
		parser.accepts("region-count");
		parser.accepts("verbose");
		return parser;
	}
	
	static void printBanner() {
		System.out.println("Kremlin Ver 0.1");
	}
	
	public static void main(String[] args) throws Exception {
		printBanner();

		OptionParser parser = configureParser();		
		OptionSet options = parser.parse(args);
		//OptionSet options = parser.parse("kremlin", "-help");

		if (options.has("help")) {
			parser.printHelpOn(System.out);
			System.exit(0);
		}

		ArgDB db = ArgDB.getInstance(options);
		String planner = db.getPlanner();
		
		//db.path = "g:\\work\\ktest\\fft";
		//db.path = "g:\\work\\ktest\\hotspot";
		planner = "gpu";

		if (planner.equals("none")) {
			System.out.println("profiler");
			KremlinProfiler.run(db);
			
		} else if (planner.equals("openmp")) {
			System.out.println("openmp");
			KremlinOMP.run(db);
			
		} else if (planner.equals("gpu")) {
			System.out.println("gpu");
			KremlinGPU.run(db);
			
		} else {
			System.out.println("Unknown planner - " + planner);
			
		}
	}
}

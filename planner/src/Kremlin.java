import kremlin.KremlinConfig;
import joptsimple.*;
import planner.*;

public class Kremlin {
	static OptionParser configureParser() {
		OptionParser parser = new OptionParser();
		parser.accepts("help");
		parser.accepts("cores", "number of cores").withOptionalArg().describedAs("int").defaultsTo("4");
		parser.accepts("min-time-reduction").withOptionalArg().describedAs("float").defaultsTo("0.0");
		parser.accepts("planner").withOptionalArg().defaultsTo("profiler");
		parser.accepts("overhead").withOptionalArg().defaultsTo("0");
		parser.accepts("sregion").withOptionalArg().defaultsTo("0");
		//OptionSet options = parser.parse("--cores=3");
		parser.accepts("region-count");
		parser.accepts("verbose");
		return parser;
	}
	
	static void printBanner() {
		System.out.println("Kremlin Planner Ver 0.2");
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

		KremlinConfig.configure(options);		
		PlannerType type = KremlinConfig.getPlanner();		
		System.out.println("PlannerType = " + type);
		
		switch(type) {
		case Profiler:			 
			KremlinProfiler.run();
			break;

		case Query:			 
			KremlinQuery.run();
			break;

		case GPU:			 
			KremlinGPU.run();
			break;
			
		case OpenMP:			 
			KremlinOMP.run();
			break;
			
		case Cilk:
			KremlinCilk.run();
			break;			
		
		default:
			System.out.println("Unsupported planner - " + PlannerType.Cilk);	 
		}		
	}
}

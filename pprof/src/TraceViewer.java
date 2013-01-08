
import kremlin.*;
public class TraceViewer {
	public static void main(String args[]) {
		//String file = "g:\\work\\ktest\\recursion\\kremlin.bin";
		if (args.length == 0) {
			System.out.println("Usage: TraceViewer kremlin.bin\n");
			System.exit(0);
		}
		TraceReader reader = new TraceReader(args[0]);
		//TraceReader reader = new TraceReader(file);
		reader.dump();
		
	}
}

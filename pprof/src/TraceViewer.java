
import pprof.*;
public class TraceViewer {
	public static void main(String args[]) {
		TraceReader reader = new TraceReader(args[0]);
		reader.dump();
		
	}
}

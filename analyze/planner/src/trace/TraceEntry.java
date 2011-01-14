package trace;


public class TraceEntry implements Comparable {
	String module;
	long startLine;
	long time;
	
	public TraceEntry(String module, long startLine, long timestamp) {
		this.module = module;
		this.startLine = startLine;
		this.time = timestamp;
	}
	
	public boolean accepts(RegionKey key) {
		if (key.getModule().equals(module) && key.getStartLine() == startLine)
			return true;
		else
			return false;
	}
	
	public long getTime() {
		return time;
	}
	
	public String getModule() {
		return module;
	}
	
	public long getStartLine() {
		return startLine;
	}

	@Override
	public int compareTo(Object o) {
		TraceEntry target = (TraceEntry)o;
		if (this.time - target.time > 0)
			return -1;
		else 
			return 1;
	}
	
	public String toString() {
		return String.format("%s [%5d]: %d", module, startLine, time);
	}
}

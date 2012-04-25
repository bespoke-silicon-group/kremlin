package trace;

public class RegionKey {
	String module;
	long startLine;
	
	RegionKey(String str) {
		String[] splitted = str.split(":");		
		this.module = splitted[0].trim();
		this.startLine = Integer.parseInt(splitted[1].trim());
	}
	
	RegionKey(String module, long startLine) {
		this.module = module;
		this.startLine = startLine;
	}
	
	public String getModule() {
		return module;
	}
	
	public long getStartLine() {
		return startLine; 
	}
	
	public String toString() {
		return String.format("%s:%d", module, startLine);
	}
	
	public boolean equals(Object to) {
		RegionKey target = (RegionKey)to;
		if (target.getModule().equals(this.module) && 
				target.getStartLine() == this.startLine)
			return true;
		return false;
	}
}

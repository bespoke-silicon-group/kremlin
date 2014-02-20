package kremlin;

/*
 * class that represents a static program region.
 */
public class SRegion implements Comparable {	
	long id;
	//boolean isFunction;
	RegionType type;
	int startLine, endLine;
	String name;
	String module;
	String func;
	
	SRegion(long id, String name, String module, String func, int start, int end, RegionType type) {
		this.id = id;
		this.name = name;
		this.module = module;
		this.func = func;
		this.startLine = start;
		this.endLine = end;
		this.type = type;
		//this.isFunction = isFunction;
	}
	
	SRegion(int id, String name, String module, int start, int end) {
		this.id = id;
		this.name = name;
		this.startLine = start;
		this.endLine = end;
		this.module = module;
		this.func = null;				
	}
	
	static public String toHeaderString() {
		return String.format("[ id]   type %15s [start - end]:  %15s",
				"module", "func");
	}
	
	public String toString() {		
		return String.format("%6s %10s [%4d - %4d]:  %10s", 
				type, module, startLine, endLine, this.func);
	}
	
	public RegionType getType() {
		return this.type;
	}
	
	public boolean isFunction() {
		return (type == RegionType.FUNC);
	}
	
	void setFuncRegion() {
		this.type = RegionType.FUNC;
	}
	
	void setFuncName(String name) {
		this.func = name;
	}
	
	public String getFuncName() {
		return this.func;
	}
	
	public int getStartLine() {
		return startLine;
	}
	
	public int getEndLine() {
		return endLine;
	}
	
	public String getModule() {
		return this.module;
	}
	
	public long getID() {
		return this.id;
	}

	@Override
	public int compareTo(Object o) {
		SRegion target = (SRegion)o;		
		return (int)(target.id - this.id);
	}
}

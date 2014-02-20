package kremlin;

/*
 * A class that represents a callsite type of SRegion.
 */
public class CallSite extends SRegion {
	CallSite(long id, String name, String module, String func, int start, int end, RegionType type) {
		super(id, name, module, func, start, end, type);
	}
	
	public String toString() {
		return String.format("called from %s line %d", module, this.startLine);
	}
}

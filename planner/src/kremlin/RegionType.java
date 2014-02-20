package kremlin;

/*
 * Enum to represent the type of a region.
 * Options include: function, loop, loop body, and callsite.
 */
public enum RegionType {
	FUNC(0), LOOP(1), BODY(2), CALLSITE(3);
	
	private int code;
	
	private RegionType(int c) {
		this.code = c;
	}
	
	public int getCode() { return code; }
}

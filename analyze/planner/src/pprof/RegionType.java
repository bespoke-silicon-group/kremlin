package pprof;

public enum RegionType {
	FUNC(0), LOOP(1), BODY(2);
	
	private int code;
	
	private RegionType(int c) {
		this.code = c;
	}
	
	public int getCode() {
		return code;
	}
}

package kremlin;

public enum RegionStatus {
	SERIAL(0), PARALLEL(1), EXCLUDED(2), SUPPRESSED(3);
	
	private int code;
	
	private RegionStatus(int c) {
		this.code = c;
	}
	
	public int getCode() {
		return code;
	}
}

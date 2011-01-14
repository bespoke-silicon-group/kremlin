package target;

public class SpeedupRecord {
	String desc;
	int nCore;
	double speedup;
	
	SpeedupRecord(String desc, int nCore, double speedup) {
		this.desc = desc;
		this.nCore = nCore;
		this.speedup = speedup;
	}
	
	public String toString() {
		String ret = String.format
			("%25s : %8.3f X at %4d cores", desc, speedup, nCore);
		return ret;
	}
}

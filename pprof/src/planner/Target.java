package planner;

public class Target {
	int overhead;
	int numCore;
	int clockMHz;
	int bwMB;
	int cacheMB;	
	
	public Target(int numCore, int overhead) {
		this.numCore = numCore;
		this.overhead = overhead;
	}
	
	public String toString() {
		return String.format("NumCore = %d, Overhead = %d,  Cache = %d MB, BW= %d MB/s", 
				numCore, overhead, cacheMB, bwMB);
	}
	
	public void setClock(int clock) {
		this.clockMHz = clock;
	}
	
	public void setBandwidth(int bw) {
		this.bwMB = bw;
	}
	
	public void setCache(int cache) {
		this.cacheMB = cache;
	}
	
	public int getClockMHz() {
		return this.clockMHz;
	}
	
	public int getBandwidthMB() {
		return this.bwMB;
	}
	
	public int getCacheMB() {
		return this.cacheMB;
	}
}

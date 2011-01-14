package pyrplan;

import pprof.*;

public class RegionRecord {
	/*
	SRegionInfo info;
	long before, after;
	double savingPercent;
	
	public RegionRecord(SRegionInfo info){
		this.info = info;
		//this.before = before;
		//this.after = after;
	}
	
	public void setTimeSaving(double savingPercent) {
		this.savingPercent = savingPercent;
	}
	
	public double getTimeSaving() {
		return this.savingPercent;
	}
	
	public String toString() {
		//double speedup = this.before / (double)this.after;
		//return String.format("%.2f\t%s", speedup, info.getSRegion());
		return String.format("%.2f\t%s", savingPercent, info.getSRegion());
	}
	
	public SRegionInfo getRegionInfo() {
		return info;
	}
	*/
	SRegionInfo info;
	double speedup;
	double timeSave;
	long expectedExecTime;
	int nCore;		
	public RegionRecord(SRegionInfo info, int nCore, double speedup) {
		this.info = info;
		this.nCore = nCore;
		this.speedup = speedup;
		//this.timeSave = timeSave;
	}
	
	public String toString() {
		return String.format("%s core=%d speedup=%.2f expectedTime = %d", 
				info.getSRegion(), nCore, speedup, expectedExecTime);
	}
	
	public SRegionInfo getRegionInfo() {
		return info;
	}
	
	public int getCoreCount() {
		return nCore;
	}
	/*
	public double getTimeSave() {
		return timeSave;
	}*/
	
	
	public long getExpectedExecTime() {
		return expectedExecTime;
	}
	
	public void setExpectedExecTime(long time) {
		this.expectedExecTime = time;
	}
	
	public double getSpeedup() {
		return this.speedup;
	}
}

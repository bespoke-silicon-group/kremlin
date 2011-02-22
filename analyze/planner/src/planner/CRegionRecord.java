package planner;

import pprof.*;

/**
 * CRegionRecord is used for planners where 
 * each CRegionRecord can be treated independently
 * regardless of parallelization status of other regions
 * 
 * @author dhjeon
 *
 */

public class CRegionRecord implements Comparable {	
	CRegion info;
	//double speedup;
	double timeSave;
	//long expectedExecTime;
	int nCore;		
	public CRegionRecord(CRegion info, int nCore, double timeSave) {
		this.info = info;
		this.nCore = nCore;
		//this.speedup = speedup;
		this.timeSave = timeSave;
		System.out.printf("%.2f %s\n", timeSave, info);
	}
	
	public String toString() {
		return String.format("TimeSave=%.2f %s at core=%d", 
				timeSave, info.getSRegion(), nCore);
	}
	
	public CRegion getCRegion() {
		return info;
	}
	
	public int getCoreCount() {
		return nCore;
	}
	
	public double getTimeSave() {
		return timeSave;
	}
	/*
	public double getTimeSave() {
		return timeSave;
	}*/
	@Override
	public int compareTo(Object arg) {
		CRegionRecord target = (CRegionRecord)arg;		
		double diff = target.timeSave - timeSave;
		return (diff > 0.0) ? 1 : -1; 
	}	
}

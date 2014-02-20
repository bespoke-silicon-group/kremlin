package planner;

import kremlin.*;

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
	double timeSave;
	int nCore;

	public CRegionRecord(CRegion info, int nCore, double timeSave) {
		this.info = info;
		this.nCore = nCore;		
		this.timeSave = timeSave;
		
	}
	
	public String toString() {
		return String.format("TimeSave=%.2f %s at core=%d", 
				timeSave, info.getSRegion(), nCore);
	}
	
	/* Getters */
	public CRegion getCRegion() { return info; }
	public int getCoreCount() { return nCore; }
	public double getTimeSave() { return timeSave; }

	@Override
	public int compareTo(Object arg) {
		CRegionRecord target = (CRegionRecord)arg;		
		double diff = target.timeSave - timeSave;
		return (diff > 0.0) ? 1 : -1; 
	}	
}

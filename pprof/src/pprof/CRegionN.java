package pprof;

public class CRegionN extends CRegion {
	CRegionN(SRegion sregion, CallSite callSite, TraceEntry entry) {
		super(sregion, callSite, entry);
		assert(entry.statList.size() == 1);
		this.stat = entry.statList.get(0);
	}
	
	CRegionStat stat;	
	
	public double getSelfP() {	
		return stat.getSelfP();
	}
	
	public double getMinSelfP() {
		return stat.getMinSelfP();		
	}
	
	public double getMaxSelfP() {
		return stat.getMaxSelfP();		
	}

	@Override
	public long getTotalWork() {		
		return stat.getTotalWork();
	}

	@Override
	public long getAvgWork() {		
		return stat.getAvgWork();
	}

	public String getStatString() {		
		String stats = null;
		
		stats = String.format("sp = %5.2f [%5.2f - %5.2f] count = %d",
				getSelfP(), getMinSelfP(), getMaxSelfP(), getInstanceCount());		 
			
		if (this.getSRegion().getType() == RegionType.LOOP) {
			stats += String.format("iter = %.2f [%d - %d]", stat.getAvgIter(), stat.getMinIter(), stat.getMaxIter());					
		}			
		
		return stats;
	}
}

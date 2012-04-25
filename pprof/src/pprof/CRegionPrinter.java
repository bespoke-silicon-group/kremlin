package pprof;

import java.util.*;

public class CRegionPrinter {
	CRegionManager manager;
	public CRegionPrinter(CRegionManager manager) {
		this.manager = manager;
	}
	
	String contextFuncString(CRegion region) {
		SRegion sregion = region.getSRegion();
		assert(sregion.getType() == RegionType.FUNC);
		CallSite callsite = region.callSite;
		String module = "root";
		int startLine = 0;
		
		if (callsite != null) {
			module = callsite.module;
			startLine = callsite.startLine;
		}
		return String.format("%s called at file %10s, line %4d", sregion, module, startLine); 
	}
	
	public String getContextString(CRegion region) {
		StringBuffer buf = new StringBuffer();
		String targetString = region.getSRegion().toString();
		if (region.getSRegion().getType() == RegionType.FUNC)
			targetString = contextFuncString(region);
		buf.append(targetString + "\n");
		CRegion current = region.getParent();
		while (current != null  && current != manager.getRoot()) {
			SRegion sregion = current.getSRegion();
			if (sregion.getType() == RegionType.FUNC && current.callSite != null) {				
				buf.append(contextFuncString(current) + "\n");
			}
			current = current.getParent();
		}		
		return buf.toString();
	}
	
	public String getStatString(CRegion region) {		
		assert(region != null);
		assert(region.type != null);
		double timeReduction = manager.getTimeReduction(region);
		String stats = String.format("ID = %d, IdealTimeReduction = %5.2f%% , type = [%s, %s], cov = %5.2f%%",				
				region.id, timeReduction, region.getParallelismType(), region.type.toString(), manager.getCoverage(region));
		return stats;
	}
	
	
	public String getString(CRegion region) {		
		String context = getContextString(region);
		String stats = getStatString(region);
		String ret = String.format("%s\t\t%s", context, stats);
		return ret;		
	}
	
	public void printRegionList(List<CRegion> list) {
		int index = 0;
		long total = 0;
		for (CRegion region : list) {
			System.out.printf("[%3d] %s %s\n%s\n", 
					index++, getStatString(region), region.getStatString(), getContextString(region));
			total += region.numInstance;
		}
		System.out.printf("Total Region Count = %d\n", total);
	}
}

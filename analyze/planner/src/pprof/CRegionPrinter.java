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
		double coverage = manager.getCoverage(region);
		double timeReduction = manager.getTimeReduction(region);
		String stats = String.format("IdealExecTimeReduction = %5.2f%% coverage = %5.2f%%, sp = %5.2f [%5.2f - %5.2f], tp = %.2f", 
				timeReduction, coverage, region.selfParallelism, region.minSP, region.maxSP, region.totalParallelism);
		return stats;
	}
	
	public String getStatString2(CRegion region) {		
		String stats = String.format("avg work = %.2f, cp = %.2f, count = %5d, readCnt = %.2f, writeCnt = %.2f", 
				region.getAvgWork(), region.getAvgCP(), region.getInstanceCount(), region.getAvgReadCnt(), region.getAvgWriteCnt());
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
		for (CRegion region : list) {
			System.out.printf("[%3d] %s\n      %s\n%s\n", 
					index++, getStatString(region), getStatString2(region), getContextString(region));
		}
	}
}

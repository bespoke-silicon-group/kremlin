package pyrplan.analyzer;

import pprof.*;

import java.util.*;

public class GprofAnalyzer {
	SRegionInfoAnalyzer analyzer;
	public GprofAnalyzer(SRegionInfoAnalyzer analyzer) {
		this.analyzer = analyzer;
	}
	
	List<SRegionInfo> recommend(double minCoverage) {
		List<SRegionInfo> ret = new ArrayList<SRegionInfo>();
		List<SRegionInfo> list = analyzer.getGprofList();
		long totalCnt = 0;
		long hitCnt = 0;
		for (SRegionInfo info : list) {
			
			totalCnt++;
			
			if (info.getCoverage() > minCoverage) {
				hitCnt++;
				ret.add(info);
			}			
		}
		
		
		double percent = (hitCnt / (double)totalCnt) * 100.0;
		System.out.printf("GprofAnalyzer %d/%d = %.2f%%\n", hitCnt, totalCnt, percent);
		return ret;
	}
}

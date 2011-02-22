package planner;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.*;

import pprof.SRegionInfo;
import pprof.SRegionInfoAnalyzer;

public class RefPlanner extends Planner {	
	public RefPlanner(SRegionInfoAnalyzer analyzer) {
		super(analyzer);
	}
	
	List<SRegionInfo> recommend(double minSpeedup) {
		List<SRegionInfo> ret = new ArrayList<SRegionInfo>();
		List<SRegionInfo> list = analyzer.getGprofList();
		long totalCnt = 0;
		long hitCnt = 0;
		for (SRegionInfo info : list) {
			if (info.isExploited()) {
				totalCnt++;
				
				if (info.getSelfSpeedup() > minSpeedup) {
					hitCnt++;
					ret.add(info);
				}
			}
		}		
		SRegionInfoSorter.sortBySpeedup(ret);
		double percent = (hitCnt / (double)totalCnt) * 100.0;
		//System.out.printf("RefAnalyzer %d/%d = %.2f%%\n", hitCnt, totalCnt, percent);
		SRegionInfoSorter.sortBySpeedup(ret);
		this.setPlan(ret);
		return ret;
	}
	
	
}

package pprof;

import visualizer.*;
import java.util.*;

public class DrawSetGenerator {
	SRegionInfoAnalyzer analyzer;
	DrawSetGenerator(SRegionInfoAnalyzer analyzer) {
		this.analyzer = analyzer;
	}
	
	DrawSet getDrawSet(SRegion region) {
		
		SRegionInfo parentInfo = analyzer.getSRegionInfo(region);
		DrawSet ret = new DrawSet((long)(parentInfo.totalParallelism), 100);
		long totalWork = parentInfo.totalWork;
		
		Set<SRegion> children = parentInfo.children;
		for (SRegion child : children) {
			SRegionInfo childInfo = analyzer.getSRegionInfo(child);
			double work = (double)childInfo.workCoverage / parentInfo.workCoverage * 100.0;
			ret.addChild((long)childInfo.totalParallelism, (long)work);
			
		}
		
		return ret;
	}
}

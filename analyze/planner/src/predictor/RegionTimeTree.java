package predictor;
import pprof.*;
import trace.TraceEntry;
import trace.TraceEntryManager;

import java.util.*;

public class RegionTimeTree {
	//Map<SRegionInfo, Long> map;
	List<PElement> list;
	
	public RegionTimeTree(String timeFile, String regionFile, SRegionInfoAnalyzer analyzer) {
		//map = new HashMap<SRegionInfo, Long>();
		list = new ArrayList<PElement>();
		//TraceEntryManager timeMap = RegionTimeReader.extractRegionTimeMap(timeFile, regionFile);
		TraceEntryManager timeMap = TraceEntryManager.buildTraceEntryManager(timeFile, regionFile);
		buildTree(timeMap, analyzer);
	}
	
	public RegionTimeTree(String regionFile, SRegionInfoAnalyzer analyzer) {
		list = new ArrayList<PElement>();
		TraceEntryManager timeMap = TraceEntryManager.buildTraceEntryManager(regionFile, analyzer);
		buildTree(timeMap, analyzer);		
	}
	
	
	
	void buildTree(TraceEntryManager manager, SRegionInfoAnalyzer analyzer) {
		Set<SRegionInfo> set = analyzer.getSRegionInfoSet();
		//for (long startLine : regionTimeMap.keySet()) {
		TraceEntry root = manager.getRootEntry();
		for (TraceEntry entry : manager.getTraceEntrySet()) {			
			if (entry.getTime() == 0L)
				continue;
			
			SRegionInfo found = null;			
			if (entry == root) {
				found = analyzer.getRootInfo();
			} else
				found = analyzer.getSRegionInfoByLine(entry.getModule(), entry.getStartLine());
			
			if (found == null) {
				System.err.println("region with startline " + entry.getStartLine() + " not found");
			}
			assert(found != null);
			PElement element = new PElement(found, entry.getTime());
			list.add(element);
		}
		Collections.sort(list);
		
		for (PElement each : list)
			System.out.println(each);
	}
	
	public PElement getTreeElement(int index) {
		return list.get(index);
	}
	
	public int getTreeSize() {
		return list.size();
	}
	
	
	
	
}

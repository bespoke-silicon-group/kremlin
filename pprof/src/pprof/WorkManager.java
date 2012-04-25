package pprof;

import java.util.*;

public class WorkManager{
	
	RegionManager manager;
	Map<DRegion, Long> exclusiveWorkMap;
	
	WorkManager(RegionManager manager) {
		this.manager = manager;
		this.exclusiveWorkMap = new HashMap<DRegion, Long>();
		
		for (DRegion each : manager.getDRegionSet()) {
			long sumChildrenWork = 0;
			for (DRegion child : each.children) {
				sumChildrenWork += child.getWork();
			}
			
			long exclusiveWork = each.getWork() - sumChildrenWork;
			exclusiveWorkMap.put(each, exclusiveWork);
		}
	}
	
	long getExclusiveWork(DRegion region) {
		return exclusiveWorkMap.get(region);
	}
}

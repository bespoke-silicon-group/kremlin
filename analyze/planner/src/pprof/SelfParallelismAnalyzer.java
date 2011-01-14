package pprof;
import java.util.*;

public class SelfParallelismAnalyzer {
	EntryManager manager;
	Map<URegion, Double> map;
	
	SelfParallelismAnalyzer(EntryManager manager) {
		this.manager = manager;
		this.map = new HashMap<URegion, Double>();
		build();
	}
	
	double getRP(URegion entry) {
		assert(map.containsKey(entry));
		return map.get(entry);
	}
	
	void build() {
		SRegionManager sManager = manager.getSRegionManager();
		
		for (SRegion sregion : sManager.getSRegionSet()) {
			Set<URegion> set = manager.getDEntrySet(sregion);
			for (URegion each : set) {
				double rp = calculateRP(each);
				map.put(each, rp);
			}
		}
	}
	/*
	double calculateCoverage(DEntry entry) {
		long sum = 0;
		for (DEntry parent : entry.parentSet) {
			long cnt = parent.getChildCount(entry);
			sum += cnt * entry.work
			serial += child.cp * cnt;
			childrenTotal += child.work * cnt;
		}
	}*/
	
	double calculateRP(URegion entry) {
		if (entry.cp == 0)
			return 0;
		
		if (entry.getChildrenSet().size() == 0) {
			return entry.work / (double)entry.cp;
		}
		
		long serial = 0;
		long childrenTotal = 0;
		for (URegion child : entry.getChildrenSet()) {
			long cnt = entry.getChildCount(child);
			serial += child.cp * cnt;
			childrenTotal += child.work * cnt;
		}
		long exclusive = entry.work - childrenTotal;
		serial += exclusive;		
		
		return  serial / (double)entry.cp;
	}
	
	
}

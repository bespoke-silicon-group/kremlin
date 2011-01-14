package pprof;

import java.util.*;
import java.util.Map;
import java.util.Set;

public class FreqAnalyzer {
	EntryManager manager;
	Map<URegion, Long> map;
	
	FreqAnalyzer(EntryManager manager) {
		this.manager = manager;
		this.map = new HashMap<URegion, Long>();
		build();
	}
	
	void build() {
		SRegionManager sManager = manager.getSRegionManager();
		
		List<URegion> readyList = new LinkedList<URegion>();
		readyList.add(manager.root);
		
		Set<URegion> retired = new HashSet<URegion>();
		
		while(readyList.isEmpty() == false) {
			URegion toRemove = readyList.remove(0);
			
			if (retired.containsAll(toRemove.parentSet) == false) {
				readyList.add(toRemove);
				continue;
			}
			
			if (toRemove.parentSet.size() == 0) {
				assert(toRemove == manager.root);
				map.put(toRemove, 1L);
				
				for (URegion child : toRemove.children.keySet()) {
					readyList.add(child);
				}
				retired.add(toRemove);
				continue;
			}
			
			long sum = 0;
			for (URegion parent : toRemove.parentSet) {
				assert(retired.contains(parent));
				long cnt = map.get(parent);
				long weight = parent.getChildCount(toRemove);
				sum += cnt * weight;
			}
			map.put(toRemove, sum);	
			retired.add(toRemove);
			
			for (URegion child : toRemove.children.keySet()) {
				readyList.add(child);
			}
		}		
	}
	
	long getFreq(URegion entry) {
		assert(map != null && entry != null);
		assert(map.containsKey(entry));
		return map.get(entry);
	}
	
	double getCoveragePercentage(URegion entry) {
		long freq = getFreq(entry);
		long total = freq * entry.work;
		return ((double)total / manager.getRoot().work) * 100.0;
	}
}

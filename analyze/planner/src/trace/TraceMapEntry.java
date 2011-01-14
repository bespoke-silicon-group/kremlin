package trace;

import java.util.*;

public class TraceMapEntry implements Comparable {
	List<RegionKey> serialKey;
	List<RegionKey> parallelKey;
	Map<Integer, Set<TraceEntry>> map;	// core <-> Set<TraceEntry> map
	
	TraceMapEntry(List<RegionKey> serial, List<RegionKey> parallel) {
		this.serialKey = serial;
		this.parallelKey = parallel;
		this.map = new HashMap<Integer, Set<TraceEntry>>();
	}
		
	
	void extractSerialTrace(TraceEntryManager manager) {
		extractTrace(0, serialKey, manager);
	}
	
	void extractParallelTrace(TraceEntryManager manager, int numCore) {
		extractTrace(numCore, parallelKey, manager);
	}
	
	void extractTrace(int numCore, List<RegionKey> keyList, TraceEntryManager manager) {
		Set<TraceEntry> set = new HashSet<TraceEntry>();
		for (RegionKey key : keyList) {
			TraceEntry entry = manager.getTraceEntry(key);
			if (entry != null) {
				set.add(entry);
			}
		}
		map.put(numCore, set);
	}
	
	long getTotalTime(int nCore) {
		Set<TraceEntry> set = map.get(nCore);
		long total = 0;
		
		for (TraceEntry each : set) {
			total += each.time;
		}
		return total;
	}
	
	double getSpeedup(int nCore) {
		long serialTime = getTotalTime(0);
		long parallelTime = getTotalTime(nCore);
		//System.out.printf("%d,  %d\n", serialTime, parallelTime);
		return serialTime / (double)parallelTime;
	}
	
	public String toString() {
		return String.format("%s <-> %s", serialKey, parallelKey);
	}
	
	public void dump() {
		System.out.println(this);
		for (int core : map.keySet()) {
			Set<TraceEntry> set = map.get(core);
			System.out.println(core + " cores");
			for (TraceEntry each : set)
				System.out.println("\t" + each);
		}
		
	}


	@Override
	public int compareTo(Object o) {
		TraceMapEntry target = (TraceMapEntry)o;
		if (target.getTotalTime(0) < this.getTotalTime(0))
			return -1;
		else
			return 1;		
	}
}

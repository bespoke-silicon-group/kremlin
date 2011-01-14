package pprof;

import java.util.*;

public class EntryManager {
	Map<SRegion, MapEntry> map;
	SRegionManager sManager;
	URegion root;
	
	EntryManager(SRegionManager manager) {
		this.sManager = manager;
		initDEntryMap();
		this.root = null;
	}
	
	
	
	class MapEntry {
		MapEntry() {
			this.map = new HashMap<Long, List<URegion>>();
		}
		Map<Long, List<URegion>> map;
		/*
		URegion getDEntry(long work, long cp, long readCnt, long writeCnt, Map<URegion, Long> children) {
			if (map.containsKey(work) == false)
				return null;
			
			List<URegion> list = map.get(work);
			
			for (URegion each : list) {
				if (each.isCompatible(cp, readCnt, writeCnt, children)) {
					return each;
				}
			}
			return null;
		}*/
		
		void addDEntry(URegion entry) {
			if (map.containsKey(entry.work) == false) {
				map.put(entry.work, new ArrayList<URegion>());
			}
			
			List<URegion> list = map.get(entry.work);
			for (URegion each : list) {
				//assert(each.isCompatible(entry.cp, entry.children) == false);
				assert(each.isCompatible(entry) == false);
			}			
			list.add(entry);
		}
		
		Set<URegion> getDEntrySet() {
			Set<URegion> ret = new HashSet<URegion>();
			for (List<URegion> list : map.values()) {
				ret.addAll(list);
			}
			return ret;
		}
	}
	
	public Set<URegion> getDEntrySet(SRegion region) {			
		MapEntry entry = map.get(region);
		return entry.getDEntrySet();		
	}
	
	public URegion getRoot() {
		return this.root;
	}
	
	public SRegionManager getSRegionManager() {
		return this.sManager;
	}
	
	void addDEntry(URegion toAdd) {
		MapEntry entry = map.get(toAdd.sregion);
		assert(entry != null);		
		entry.addDEntry(toAdd);		
		/*
		for (DEntry child : toAdd.children.keySet()) {			
			child.setParent(toAdd);
		}*/
	}
	
	void initDEntryMap() {
		map = new HashMap<SRegion, MapEntry>();
		for (SRegion sregion : sManager.getSRegionSet()) {
			//List<DEntry> list = new ArrayList<DEntry>();
			//map.put(sregion, list);
		
			MapEntry entry = new MapEntry();
			map.put(sregion, entry);			
		}
	}
}

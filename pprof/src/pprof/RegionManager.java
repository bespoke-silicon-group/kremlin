package pprof;
import java.util.*;

public class RegionManager {
	RegionManager(Map<Long, SRegion> sMap, Map<SRegion, Set<DRegion>> dMap) {
		this.dMap = dMap;
		this.dSet = new HashSet<DRegion>();
		this.root = null;
		this.leafSet = new HashSet<DRegion>();
		
		for (SRegion each : dMap.keySet()) {
			for (DRegion region : dMap.get(each)) {
				dSet.add(region);
				if (region.parent == null) {
					System.out.println(region);
					assert(this.root == null);					
					this.root = region;
				}
				
				if (region.children.size() == 0)
					leafSet.add(region);
			}
		}
		
		this.sMap = new HashMap<Long, SRegion>();
		for (SRegion each : sMap.values()) {
			if (dMap.get(each).size() > 0)
				this.sMap.put(each.id, each);
		}
		assert(this.root != null);
	}
	
	Map<SRegion, Set<DRegion>> dMap;
	Map<Long, SRegion> sMap;
	Set<DRegion> dSet;
	Set<DRegion> leafSet;
	DRegion root;
	
	public List<SRegion> getSRegionList() {
		List<SRegion> ret = new ArrayList<SRegion>();
		for (long key : sMap.keySet())
			ret.add(sMap.get(key));
		return ret;
	}
	
	public Set<SRegion> getSRegionSet() {
		return new HashSet<SRegion>(sMap.values());
	}
	
	Set<DRegion> getDRegionSet() {
		return new HashSet<DRegion>(dSet);
	}
	
	Set<DRegion> getDRegionSet(SRegion region) {
		if (!dMap.containsKey(region)) {
			System.out.println("No dynamic region found for : ");
			System.out.println("\t" + region);
			assert(false);
		}
		return new HashSet<DRegion>(dMap.get(region));
	}
	
	DRegion getDRegion(int sid, int did) {
		SRegion sregion = sMap.get(sid);
		if (sregion == null) {
			System.out.println("SRegion " + sid + " cannot be found");
			System.out.println(sMap);
		}
		assert(sregion != null);
		
		Set<DRegion> dregionSet = dMap.get(sregion);
		assert(dregionSet != null);
		for (DRegion each : dregionSet) {
			if (each.did == did) {
				//System.out.println(each);
				return each;
			}
		}
		//System.out.println("Cannot load sid " + sid + " did " + did);
		//assert(false);
		return null;		
	}
	
	SRegion getSRegion(int id) {
		return sMap.get(id);
	}
	
	SRegion getSRegion(String name) {
		for (SRegion each : sMap.values()) {			
			assert(each != null);
			assert(each.name != null);
			if (each.name.equals(name))
				return each;
		}
		return null;
	}
	
	boolean hasSRegion(int id) {
		return sMap.containsKey(id);
	}
	 
	DRegion getRoot() {
		return root;
	}
	
	void dump() {
		for (SRegion key : dMap.keySet()) {
			
		}
	}
	
	boolean isSelfCheckFine() {
		Map<Long, SRegion> smap = new HashMap<Long, SRegion>();
		for (DRegion each : dSet) {
			if (!smap.containsKey(each.sregion.id)) {
				smap.put(each.sregion.id, each.sregion);
			}
			
			SRegion sregion = smap.get(each.sregion.id);
			assert(sregion == each.sregion);				
		}
		return true;
	}
	
	
}

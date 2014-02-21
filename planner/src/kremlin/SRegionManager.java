package kremlin;

import java.io.*;
import java.util.*;

/*
 * A class to track all the static regions of the program.
 */
public class SRegionManager {
	Map<Long, SRegion> sMap;
	Map<Long, CallSite> callSiteMap;
	boolean isNeo = false;

	public SRegionManager(File file, boolean newVersion) {
		this.isNeo = newVersion;
		sMap = new HashMap<Long, SRegion>();
		callSiteMap = new HashMap<Long, CallSite>();
		SRegion rootRegion = new SRegion(0, "root", "root", "root", 0, 0, RegionType.LOOP);
		sMap.put(0L, rootRegion);
		
		parseSRegionFile(file);
	}
	
	/*
	 * Returns set of sregions to which we have mappings.
	 */
	public Set<SRegion> getSRegionSet() {
		return new HashSet<SRegion>(sMap.values());
	}
	
	/*
	 * Returns set of sregions to which we have mappings and which are of the
	 * specified type of region.
	 */
	public Set<SRegion> getSRegionSet(RegionType type) {
		Set<SRegion> ret = new HashSet<SRegion>();
		for (SRegion each : sMap.values()) {
			if (each.getType() == type)
				ret.add(each);
		}
		return ret;
	}
	
	/*
	 * Given a static ID, returns the associated SRegion object.
	 */
	public SRegion getSRegion(long id) {		
		if (!sMap.containsKey(id)) {
			System.out.println("invalid sid: " + id);
			assert(false);
		}
		assert(sMap.containsKey(id));
		return sMap.get(id);
	}
	
	/*
	 * Given a static ID, returns the associated CallSite object.
	 */
	CallSite getCallSite(long id) {
		if (callSiteMap.containsKey(id) == false) {
			System.err.printf("callsite %16x not found\n", id);
		}
		assert(callSiteMap.keySet().contains(id));
		return callSiteMap.get(id);
	}
	
	/*
	 * Parses static region file (usually sregions.txt) to create a mapping of
	 * static IDs to SRegion objects.
	 * If the region is a CALLSITE then it is put into a mapping of static IDs
	 * to CallSite objects instead of the aforementioned mapping.
	 */
	void parseSRegionFile(File file) {
		try {
			BufferedReader input =  new BufferedReader(new FileReader(file));
			
			while(true) {
				String line = input.readLine();
				if (line == null)
					break;
				SRegion entity = SRegionReader.createSRegion(line, this.isNeo);
				if (entity == null)
					continue;
				
				if (entity.getType() == RegionType.CALLSITE) {
					callSiteMap.put(entity.id, (CallSite)entity);
				} else {
					sMap.put(entity.id, entity);
				}
			}
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}

	/*
	SRegion createSRegion(String line) {
		StringTokenizer tokenizer = new StringTokenizer(line);
		List<String> list = new ArrayList<String>();
				
		while (tokenizer.hasMoreTokens()) {
			list.add(tokenizer.nextToken());
		}
		//System.out.println(list);
		int id = Integer.parseInt(list.get(0));
		String type = list.get(1);
		String module = list.get(2);
		String func = list.get(3);		
		int start = Integer.parseInt(list.get(4));
		int end = Integer.parseInt(list.get(5));
		//String name = labelMap.get(id);
		String name = "N/A";
		if (name == null) {
			//System.out.println("\n[Error] Region id " + id + " does not have its corresponding entry in label-map.txt");			
			//assert(false);
		}
		
		//System.out.printf("%d %s %s %s %d %d\n", id, type, module, func, start, end);
		SRegion ret = new SRegion(id, name, module, func, start, end, getType(type));
		return ret;
	}*/
	
	
	/*
	 * Prints out all the SRegions.
	 */
	public void dump() {
		for (SRegion each : this.sMap.values()) {
			System.out.println(each);
		}
	}
}

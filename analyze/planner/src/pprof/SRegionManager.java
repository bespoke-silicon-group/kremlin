package pprof;

import java.io.*;
import java.util.*;

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
	
	public Set<SRegion> getSRegionSet() {
		return new HashSet<SRegion>(sMap.values());
	}
	
	public Set<SRegion> getSRegionSet(RegionType type) {
		Set<SRegion> ret = new HashSet<SRegion>();
		for (SRegion each : sMap.values()) {
			if (each.getType() == type)
				ret.add(each);
		}
		return ret;
	}
	
	public SRegion getSRegion(long id) {		
		if (!sMap.containsKey(id)) {
			System.out.println("invalid sid: " + id);
			assert(false);
		}
		assert(sMap.containsKey(id));
		return sMap.get(id);
	}
	
	CallSite getCallSite(long id) {
		assert(callSiteMap.keySet().contains(id));
		return callSiteMap.get(id);
	}
	
	//void parseSRegionFile(String file) {
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
	
	
	public void dump() {
		for (SRegion each : this.sMap.values()) {
			System.out.println(each);
		}
	}
}

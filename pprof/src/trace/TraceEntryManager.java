package trace;

import java.util.*;

import planner.Util;
import pprof.*;

public class TraceEntryManager {
	Set<TraceEntry> set;
	TraceEntry root = null;
	
	
	TraceEntryManager() {
		this.set = new HashSet<TraceEntry>();
	}
	
	public void add(String module, long startLine, long timestamp) {
		TraceEntry toAdd = new TraceEntry(module, startLine, timestamp); 
		set.add(toAdd);
		if (startLine == 0) {
			this.root = toAdd;
		}
	}
	
	public TraceEntry getRootEntry() {
		assert(root != null);
		return this.root;
	}
	
	TraceEntry getTraceEntry(RegionKey key) {
		if (key.startLine == 0)
			return this.root;
		
		for (TraceEntry entry : set) {
			if (entry.accepts(key)) {
				return entry;
			}
		}
		return null;
	}
	
	long getTimestamp(String module, long line) {
		for (TraceEntry each : set) {
			if (each.module.equals(module) && each.startLine == line)
				return each.time;
		}
		return -1;
	}
	
	long getTimestamp(RegionKey key) {
		return getTimestamp(key.module, key.startLine);		
	}
	
	List<RegionKey> getRegionKeyListDesc() {
		List<TraceEntry> list = new ArrayList<TraceEntry>(set);
		List<RegionKey> ret = new ArrayList<RegionKey>();
		Collections.sort(list);
		
		for (TraceEntry each : list) {
			ret.add(new RegionKey(each.module, each.startLine));			
		}
		return ret;
	}
	
	public Set<TraceEntry> getTraceEntrySet() {
		return new HashSet<TraceEntry>(set);
	}
	
	
	
	void print() {
		for (RegionKey each : getRegionKeyListDesc()) {
			System.out.println(each + " " + this.getTimestamp(each));
		}
	}
	
	static Map<Integer, Long> RegionTimeReader(String file) {
		Map<Integer, Long> ret = new HashMap<Integer, Long>();
		List<String> strList = Util.getStringList(file);
		for (String line : strList) {
			String[] splitted = line.split("\t");			
			int region = Integer.parseInt(splitted[1].trim());
			long time = Long.parseLong(splitted[2].trim());
			ret.put(region, time);
		}		
		return ret;
	}
	
	static List<RegionKey> readSRegionId(String regionFile) {
		List<RegionKey> ret = new ArrayList<RegionKey>();
		List<String> strList = Util.getStringList(regionFile);
		for (String line : strList) {			
			RegionKey id = new RegionKey(line);
			ret.add(id);
		}
		return ret;
	}
	
	public static TraceEntryManager buildTraceEntryManager(String timeFile, String regionFile) {
		Map<Integer, Long> timeMap = RegionTimeReader(timeFile);
		List<RegionKey> regionList = readSRegionId(regionFile);
		TraceEntryManager ret = new TraceEntryManager();
		for (int index : timeMap.keySet()) {
			long time = timeMap.get(index);
			RegionKey id;
			if (index == 0) {
				id = new RegionKey("root", 0);
			} else
				id = regionList.get(index-1);
			
			ret.add(id.getModule(), id.getStartLine(), time);
		}
		return ret;
	}
	
	public static TraceEntryManager buildTraceEntryManager(String regionFile, SRegionInfoAnalyzer analyzer) {		
		List<RegionKey> regionList = readSRegionId(regionFile);
		TraceEntryManager ret = new TraceEntryManager();
		
		long time = analyzer.getRootInfo().getTotalWork();
		ret.add("root", 0, time);
		
		for (RegionKey each : regionList) {
			String module = each.getModule();
			long line = each.getStartLine();							
			SRegionInfo info = analyzer.getSRegionInfoByLine(each.getModule(), each.getStartLine());
			if (info == null) {
				time = 0;
			} else
				time = info.getTotalWork();
							
			ret.add(module, line, time);
		}
		
		
		return ret;
	}
}

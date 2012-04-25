package trace;
import java.util.*;

import planner.Util;

public class TraceMapper {
	List<TraceMapEntry> entryList;
	//List<Integer> coreList;
	Map<Integer, TraceEntryManager> map = new HashMap<Integer, TraceEntryManager>();
	
	TraceMapper(TraceEntryManager serial, String mapFile) {
		this.entryList = new ArrayList<TraceMapEntry>();
		//this.coreList = new ArrayList<Integer>();
		map.put(0, serial);
		List<String> list = Util.getStringList(mapFile);
		for (String line : list) {
			TraceMapEntry entry = handleLine(line);
			entry.extractSerialTrace(serial);
			//entry.extractParallelTrace(parallel);
			entryList.add(entry);
		}
		Collections.sort(entryList);
	}
	
	void addParallelTrace(TraceEntryManager parallel, int nCore) {
		map.put(nCore, parallel);
		for (TraceMapEntry entry : entryList) {
			entry.extractParallelTrace(parallel, nCore);
		}
		//coreList.add(nCore);
		//Collections.sort(coreList);
	}
	
	List<RegionKey> getRegionKeyList(String str) {
		List<RegionKey> ret = new ArrayList<RegionKey>();		
		String splitted[] = str.split("\t");
		
		for (String each : splitted) {
			RegionKey key = new RegionKey(each.trim());
			ret.add(key);
		}
		return ret;
	}	
	
	TraceMapEntry handleLine(String line) {
		String splitted[] = line.split("-");
		List<RegionKey> serial = getRegionKeyList(splitted[0].trim());
		List<RegionKey> parallel = getRegionKeyList(splitted[1].trim());
		TraceMapEntry entry = new TraceMapEntry(serial, parallel);
		return entry;
	}
	
	List<Integer> getCoreList() {
		List<Integer> list = new ArrayList<Integer>(map.keySet());
		Collections.sort(list);
		list.remove(0);		
		return list;
	}
	
	void printSpeedup(String file) {
		List<String> list = new ArrayList<String>();
		for (TraceMapEntry entry : entryList) {
			StringBuffer buffer = new StringBuffer();
			for (int core : getCoreList()) {
				double speedup = entry.getSpeedup(core);
				String toAdd = String.format("%.2f\t", speedup);
				buffer.append(toAdd);
				
			}
			buffer.append(entry);
			//System.out.print("\n");
			list.add(buffer.toString());
		}
		Util.writeFile(list, file);
	}
	
	void printCoverage(String file) {
		List<String> list = new ArrayList<String>();
		for (TraceMapEntry entry : entryList) {
			StringBuffer buffer = new StringBuffer();
			for (int core : getCoreList()) {				
				long time = entry.getTotalTime(core);
				long total = map.get(core).getRootEntry().time;
				double coverage = ((double)time / total) * 100.0;				
				//System.out.printf("%.2f\t", coverage);
				String toAdd = String.format("%.2f\t", coverage);
				buffer.append(toAdd);
			}
			buffer.append(entry);
			//System.out.print(entry);
			//System.out.print("\n");
			list.add(buffer.toString());
		}
		Util.writeFile(list, file);
	}
	
	void dump() {
		for (TraceMapEntry entry : entryList) {
			entry.dump();
		}
	}
	
	public static String getFileName(String dir, String bench, int core) {
		return String.format("%s/%s.out.%d", dir, bench, core);
	}
	
	public static void main(String args[]) {		
		String resDir = "/h/g3/dhjeon/research/spaa2011/result32"; 
		String bench = "ft";
		String metaDir = "/h/g3/dhjeon/research/spaa2011/npb-exec-trace/meta";
		
		int maxCore = 32;
		
		String serialTrace = getFileName(resDir, bench, 0);
		String serialMetaFile = metaDir + "/" + bench + ".serial";
		TraceEntryManager serial = TraceEntryManager.buildTraceEntryManager(serialTrace, serialMetaFile);
		
		String parallelMetaFile = metaDir + "/" + bench + ".parallel";
		
		String mapFile = metaDir + "/" + bench + ".map";
		TraceMapper mapper = new TraceMapper(serial, mapFile);
		
		List<TraceEntryManager> managerList = new ArrayList<TraceEntryManager>();
		for (int core=1; core<=maxCore; core=core*2) {
			String trace = getFileName(resDir, bench, core);			
			TraceEntryManager parallel = TraceEntryManager.buildTraceEntryManager(trace, parallelMetaFile);
			mapper.addParallelTrace(parallel, core);
			managerList.add(parallel);
		}		
		
		String outSpeedup = resDir + "/" + bench + ".speedup";
		String outCoverage = resDir + "/" + bench + ".coverage";
		
		mapper.printSpeedup(outSpeedup);
		mapper.printCoverage(outCoverage);
		//mapper.dump();
		//serial.print();
		//parallel1.print();
		
	}
}

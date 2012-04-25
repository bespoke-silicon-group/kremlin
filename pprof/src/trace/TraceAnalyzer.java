package trace;
import java.io.*;
import java.util.*;


public class TraceAnalyzer {		
	String dir;
	String name;
	List<Integer> coreList = new ArrayList<Integer>();	
	List<RegionKey> regionList = new ArrayList<RegionKey>();
	Map<Integer, TraceEntryManager> map = new HashMap<Integer, TraceEntryManager>();	// core, TraceEntryManager
	Map<RegionKey, List<Double>> speedupMap;	// region <-> speedup list
	Map<RegionKey, List<Double>> coverageMap;	// region <-> coverage list
	
	public String getMetaFileName(String dir, String name, boolean isSerial) {
		if (isSerial)
			return dir + "/" + name + ".serial";
		else
			return dir + "/" + name + ".parallel";
	}
	
	public String getMapFileName(String dir, String name) {
		return dir + "/" + name + ".map";
	}
	
	public TraceAnalyzer(String dir, String name, String regionFileDir) {
		this.dir = dir;
		this.name = name;
		
		speedupMap = new HashMap<RegionKey, List<Double>>();
		coverageMap = new HashMap<RegionKey, List<Double>>();
		
		File file = new File(dir);
		assert(file.isDirectory() == true);
		String files[] = file.list();
		//this.fileList = new ArrayList<String>();
		for (String each : files) {
			if (each.startsWith(name + ".out")) {								
				int lastIndex = each.lastIndexOf(".");
				int numCore = Integer.parseInt(each.substring(lastIndex+1));
				coreList.add(numCore);
				//System.out.println("numCore = " + numCore);
				String fullPath = dir + "/" + each;
				boolean isSerial = (numCore == 0) ? true : false;
				String regionFile = getMetaFileName(regionFileDir, name, isSerial);
				TraceEntryManager manager = TraceEntryManager.buildTraceEntryManager(fullPath, regionFile);				
				map.put(numCore, manager);
				manager.print();
				
				//if (numCore == 0)
				//	createRegionList(manager);
			}
		}
		Collections.sort(coreList);		
		//analyze();
		//System.out.println(coreList);
	}
	
	void createRegionList(TraceEntryManager timeMap) {
		this.regionList = timeMap.getRegionKeyListDesc();
	}
	
	void analyze() {
		TraceEntryManager serialMap = map.get(1);
		//System.out.println(serialMap);
		Map<Integer, Long> appTimeMap = new HashMap<Integer, Long>(); // core, time
		for (int numCore : coreList) {
			TraceEntryManager manager = map.get(numCore);
			TraceEntry root = manager.getRootEntry();
			appTimeMap.put(numCore, root.time);			
						
		}
		System.out.println(appTimeMap);
		
		
		for (RegionKey region : this.regionList) {
			//long line = key.getStartLine();
			long serialTime = serialMap.getTimestamp(region);
			List<Long> timeList = new ArrayList<Long>();
			List<Double> speedupList = new ArrayList<Double>();
			List<Double> coverageList = new ArrayList<Double>();
			
			for (int numCore : coreList) {
				long parallelTime = map.get(numCore).getTimestamp(region);
				timeList.add(parallelTime);
				double speedup = (double)serialTime / parallelTime;
				speedupList.add(speedup);				
				assert(appTimeMap != null);
				assert(appTimeMap.containsKey(numCore));				
				//System.out.println(appTimeMap.get(numCore));
				
				double coverage = (double)parallelTime / appTimeMap.get(numCore);
				coverageList.add(coverage * 100.0);
			}
			speedupMap.put(region, speedupList);
			coverageMap.put(region, coverageList);
			//System.out.println(region + ": " + speedupList);
			//System.out.println(region + ": " + coverageList);
		}		
	}
	
	public void printTable() {
		for (RegionKey region : regionList) {
			//long region = key.getStartLine();
			StringBuffer buffer = new StringBuffer(region + ":" );
			for (int index=0; index <coreList.size(); index++) {
				int core = coreList.get(index);
				String formatted = String.format("%6.2f\t%6.2f",
						speedupMap.get(region).get(index),
						coverageMap.get(region).get(index)
						);
						
				buffer.append("\t" + formatted);				
			}
			System.out.println(buffer);
		}
	}
	
	public void printSpeedup() {
		for (RegionKey region : regionList) {			
			StringBuffer buffer = new StringBuffer(region + ":" );
			for (int index=0; index <coreList.size(); index++) {
				int core = coreList.get(index);
				String formatted = String.format("%6.2f",
						speedupMap.get(region).get(index));
						
				buffer.append("\t" + formatted);				
			}
			System.out.println(buffer);
		}
	}
	
	public void printCoverage() {
		for (RegionKey region : regionList) {			
			StringBuffer buffer = new StringBuffer(region + ":" );
			for (int index=0; index <coreList.size(); index++) {
				int core = coreList.get(index);
				String formatted = String.format("%6.2f%%",						
						coverageMap.get(region).get(index));
				buffer.append("\t" + formatted);				
			}
			System.out.println(buffer);
		}
	}
	
	public static void main(String args[]) {
		TraceAnalyzer analyzer = new TraceAnalyzer(
				"/h/g3/dhjeon/research/spaa2011/work/result", 
				"equake",
				"/h/g3/dhjeon/research/spaa2011/meta"
				);
		//analyzer.analyze();
		//analyzer.printSpeedup();
		analyzer.printCoverage();
		//analyzer.printTable();
	}
}

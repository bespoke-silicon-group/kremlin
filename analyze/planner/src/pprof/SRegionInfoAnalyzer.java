package pprof;
import java.util.*;
import java.io.File;

public class SRegionInfoAnalyzer {
	EntryManager manager;
	//Map<SRegion, Set<DRegion>> map;
	Map<SRegion, SRegionInfo> infoMap;
	FreqAnalyzer freq;
	SelfParallelismAnalyzer rp;
	SRegionInfoGroupManager groupManager;
	
	public SRegionInfoAnalyzer(EntryManager manager) {
		this.manager = manager;
		//this.map = new HashMap<SRegion, Set<DRegion>>();		
		this.infoMap = new HashMap<SRegion, SRegionInfo>();
		// warning: this is slow!
		//this.freq = new FreqAnalyzer(manager);		
		this.rp = new SelfParallelismAnalyzer(manager);		
		build();		
		//this.groupManager = new SRegionInfoGroupManager(this);		
	}
	
	public SRegionManager getSManager() {
		return this.manager.getSRegionManager();
	}
	
	public EntryManager getDManager() {
		return this.manager;
	}
	
	void build() {
		SRegion root = manager.getRoot().getSRegion();
		Set<URegion> rootSet = manager.getDEntrySet(root);
		long totalWork = rootSet.iterator().next().getWork();		
		
		for (SRegion each : manager.sManager.getSRegionSet()) {
			Set<URegion> set = manager.getDEntrySet(each);
			
			if (set.isEmpty() == false) {
				SRegionInfo info = new SRegionInfo(each, set, freq, rp, totalWork);
				infoMap.put(each, info);
			}
		}
	}
	
	public boolean isUniquified() {
		boolean ret = true;
		for (SRegionInfo info : infoMap.values()) {
			if (info.getParents().size() > 1) {
				ret = false;
				//return false;
				System.out.println(info.getSRegion());
			}
		}
		return ret;
	}
	
	void dump() {
		for (SRegionInfo info : infoMap.values()) {
			System.out.println(info);
		}
	}
	
	static final Comparator<SRegionInfo> GPROF_ORDER =
        new Comparator<SRegionInfo>() {
			public int compare(SRegionInfo e1, SRegionInfo e2) {
				return (int)(e2.getTotalWork() - e1.getTotalWork()); 
			}
	};
	
	public SRegionInfoGroup getRegionGroup(SRegion region) {
		return groupManager.getGroup(getSRegionInfo(region));
	}
	
	public void checkUniquification() {
		int cnt = 0;
		System.out.println("\nChecking Uniquification");
		for (SRegionInfo each : this.getSRegionInfoSet()) {
			if (each.getParents().size() > 1) {
				cnt++;
				
				System.out.printf("%s: parents: %d, sp: %.2f, work: %.2f\n",
						each.getSRegion(), each.getParents().size(),  each.getSelfParallelism(), each.getCoverage());
			}
		}
		System.out.println("Total " + cnt + " non-uniquified regions have been detected");
		
	}
	
	public Set<SRegion> getExploitedSet() {
		Set<SRegion> ret = new HashSet<SRegion>();
		
		for (SRegionInfo each : this.getSRegionInfoSet()) {
			if (each.isExploited())
				ret.add(each.getSRegion());
		}
		return ret;
	}
	
	public Set<SRegion> getDescendantSet(SRegion region) {
		Set<SRegion> ret = new HashSet<SRegion>();
		SRegionInfo start = this.getSRegionInfo(region);
		List<SRegion> list = new ArrayList<SRegion>(start.getChildren());
		
		while(list.size() > 0) {
			SRegion toRemove = list.remove(0);
			ret.add(toRemove);
			SRegionInfo info = this.getSRegionInfo(toRemove);
			list.addAll(info.getChildren()); 
		}
		return ret;		
	}
	
	public Set<SRegion> getAncestorSet(SRegion region) {
		Set<SRegion> ret = new HashSet<SRegion>();
		SRegionInfo start = this.getSRegionInfo(region);
		List<SRegion> list = new ArrayList<SRegion>(start.getParents());
		
		while(list.size() > 0) {
			SRegion toRemove = list.remove(0);
			ret.add(toRemove);
			SRegionInfo info = this.getSRegionInfo(toRemove);
			list.addAll(info.getParents()); 
		}
		return ret;		
	}
	
	public List<SRegionInfo> getPyrprofList() {
		List<SRegionInfo> list = new ArrayList<SRegionInfo>();		
		for (SRegionInfo each : getSRegionInfoSet()) {
			list.add(each);
		}
		Collections.sort(list);
		return list;
	}	
	
	public List<SRegionInfo> getPyrprofList(RegionType type) {
		List<SRegionInfo> list = new ArrayList<SRegionInfo>();		
		for (SRegionInfo each : getSRegionInfoSet()) {
			if (each.getSRegion().getType() == type)
				list.add(each);
		}
		Collections.sort(list);
		return list;
	}	
	
	public List<SRegionInfo> getExploitedList() {
		List<SRegionInfo> list = new ArrayList<SRegionInfo>();		
		for (SRegionInfo each : getSRegionInfoSet()) {
			if (each.isExploited())
				list.add(each);
		}
		return list;
	}
	
	public List<SRegionInfo> getGprofList() {
		List<SRegionInfo> list = new ArrayList<SRegionInfo>();
		
		for (SRegionInfo each : getSRegionInfoSet()) {
			list.add(each);
		}
		Collections.sort(list, GPROF_ORDER);
		return list;
	}
	
	public List<SRegionInfo> getGprofList(RegionType type) {
		List<SRegionInfo> list = new ArrayList<SRegionInfo>();
		
		for (SRegionInfo each : getSRegionInfoSet()) {
			if (each.getSRegion().getType() == type)
				list.add(each);
		}
		Collections.sort(list, GPROF_ORDER);
		return list;
	}
	
	public SRegionInfo getRootInfo() {
		return getSRegionInfo(this.manager.getRoot().sregion);
	}
	
	public SRegionInfo getSRegionInfo(SRegion region) {
		return infoMap.get(region);
	}
	
	// for now, choose only loop regions
	public SRegionInfo getSRegionInfoByLine(String module, long line) {
		for (SRegion each : manager.getSRegionManager().getSRegionSet(RegionType.LOOP)) {						
			if (each.getStartLine() == (int)line && module.equals(each.getModule()) ) {				
				if (getSRegionInfo(each) == null) {
					System.err.println("Warning: line " + line + " has 0 exec time from trace");
				}
				return getSRegionInfo(each);
			}
		}
		return null;
	}
	
	public Set<SRegionInfo> getSRegionInfoSet() {
		return new HashSet<SRegionInfo>(infoMap.values());
		/*
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		for (SRegion each : manager.getSRegionManager().getSRegionSet()) {
			SRegionInfo info = getSRegionInfo(each);
			if (info != null)
				ret.add(info);
		}
		return ret;*/
	}	
	
	public String formatInfoEntry(SRegion region) {
		SRegionInfo info = this.getSRegionInfo(region);
		StringBuffer buffer = new StringBuffer();				
		//boolean exploited = info.isParallelized();
		
		String numbers = String.format("%10.2f %10.2f %10.2f %10.2f",
				info.getSelfSpeedup(), info.getSelfParallelism(), info.getCoverage(), info.getNSP());   
				
		buffer.append(numbers);
		return buffer.toString();
	}
	
	public static void main(String args[]) {
		//String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/mpeg_enc";		
		String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/loop";
		System.out.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";
		
		long start = System.currentTimeMillis();
		SRegionManager sManager = new SRegionManager(new File(sFile), false);		
		URegionManager dManager = new URegionManager(sManager, new File(dFile));
		long end = System.currentTimeMillis();
		
		SRegionInfoAnalyzer analyzer = new SRegionInfoAnalyzer(dManager);
		analyzer.dump();
		
	}
	
	
}

package pprof;

import java.io.*;
import java.util.*;

import pprof.EntryManager.MapEntry;
import pyrplan.RecList;
import pyrplan.Recommender;

public class URegionManager extends EntryManager {
	Map<Long, URegion> dMap = new HashMap<Long, URegion>();
	SRegionInfoAnalyzer analyzer;
	
	public URegionManager(SRegionManager sManager, File file) {		
		super(sManager);
		try {					
			buildDEntryMap(file);
			//System.err.println("build done");
			
		} catch(Exception e) {
			if (e instanceof java.io.EOFException == false) {
				e.printStackTrace();
				assert(false);
			}
		}
		analyzer = new SRegionInfoAnalyzer(this);
	}
	
	class Entry {
		Entry(long uid, long sid, long work, long cp, 
				long readCnt, long writeCnt, long readLineCnt, long writeLineCnt,  
				long cnt, Map<Long, Long> children) {
			this.uid = uid;
			this.sid = sid;
			this.work = work;
			this.cp = cp;
			this.readCnt = readCnt;
			this.writeCnt = writeCnt;
			this.readLineCnt = readLineCnt;
			this.writeLineCnt = writeLineCnt;
			this.cnt = cnt;

			//this.pSid = pSid;
			this.children = children;
		}
		long uid, sid, work, cp, cnt, pSid;
		long readLineCnt, writeLineCnt, readCnt, writeCnt;
		Map<Long, Long> children;
		
		public String toString() {
			return String.format("uid: %d sid: %d work: %d cp: %d cnt: %d read: %d write: %d", 
					uid, sid, work, cp, cnt, readCnt, writeCnt);
		}
	}
	
	public SRegionInfoAnalyzer getSRegionAnalyzer() {
		return this.analyzer;
	}
	
	void buildDEntryMap(File file) {
		Map<Long, Entry> uMap = new HashMap<Long, Entry>();
		List<Entry> workList = new LinkedList<Entry>();
		
		try {
			DataInputStream input =  new DataInputStream(new FileInputStream(file));			
		
			while(true) {
				Map<Long, Long> childrenMap = new HashMap<Long, Long>();
				long uid = Long.reverseBytes(input.readLong());
				long sid = Long.reverseBytes(input.readLong());
				long work = Long.reverseBytes(input.readLong());
				long cp = Long.reverseBytes(input.readLong());
				long readCnt = Long.reverseBytes(input.readLong());
				long writeCnt = Long.reverseBytes(input.readLong());
				long readLineCnt = Long.reverseBytes(input.readLong());
				long writeLineCnt = Long.reverseBytes(input.readLong());				
				long cnt = Long.reverseBytes(input.readLong());				
				long nChildren = Long.reverseBytes(input.readLong());
				
				//assert(cp != 0);
				
				for (int i=0; i<nChildren; i++) {
					long childUid = Long.reverseBytes(input.readLong());
					long childCnt = Long.reverseBytes(input.readLong());
					childrenMap.put(childUid, childCnt);
				}			
				/*
				System.out.printf("%d %d %d %d %d %d (%d %d) (%d %d)\n",
						uid, sid, work, cp, cnt, nChildren, readCnt, writeCnt, readLineCnt, writeLineCnt);
				System.out.println("\t" + childrenMap);*/			
				
				uMap.put(uid, new Entry(uid, sid, work, cp, readCnt, writeCnt, readLineCnt, writeLineCnt, cnt, childrenMap));
				
				if (nChildren == 0) {
					workList.add(0, uMap.get(uid));				
				} else {
					workList.add(uMap.get(uid));
				}
			}
		} catch(Exception e) {
			//System.out.println(e);
			if (e instanceof java.io.EOFException == false) {
				e.printStackTrace();
				assert(false);
			}
		}
		Set<Entry> retired = new HashSet<Entry>();
		//System.out.println("# of CEntries = " + workList.size());
		
		
		while(workList.size() > 0) {
			Entry toRemove = workList.remove(0);
			assert(retired.contains(toRemove) == false);
			boolean isReady = true;
			for (Long uid : toRemove.children.keySet()) {
				if (retired.contains(uMap.get(uid)) == false) {
					isReady = false;
				}
			}
			
			if (isReady) {
				retired.add(toRemove);
				//System.out.println(toRemove);
				//SRegion sregion, long work, long cp, Map<DEntry, Long> map
				SRegion sregion = sManager.getSRegion(toRemove.sid);				
				assert(sregion != null);
				Map<URegion, Long> children = new HashMap<URegion, Long>();
				for (long uid : toRemove.children.keySet()) {
					assert(dMap.containsKey(uid));
					URegion child = dMap.get(uid);
					assert(child.id == uid);
					long cnt = toRemove.children.get(uid);
					children.put(child, cnt);
				}
				URegion entry = new URegion(sregion, toRemove.uid, toRemove.work, toRemove.cp, 
						toRemove.readCnt, toRemove.writeCnt, 
						toRemove.readLineCnt, toRemove.writeLineCnt,  
						toRemove.cnt, children);
				if (toRemove.cp == 0) {
					System.out.println(sregion);
					System.out.println(entry);
					/*
					toRemove.cp = 1;
					if (toRemove.work == 0)
						toRemove.work = 1;*/
				}
				//assert(toRemove.cp > 0);
				
				
				dMap.put(toRemove.uid, entry);
				//System.out.println(entry);
				//System.out.println("\t" + children + "\n");
				addDEntry(entry);
				/*
				if (toRemove.pSid == 0) {
					assert(this.root == null);
					this.root = entry;
				}*/
			} else {
				workList.add(toRemove);
			}
		}
		int cnt = 0;		
		for (URegion each : dMap.values()) {			
			if (each.parentSet.isEmpty()) {
				//assert(this.root == null);
				//System.out.println("Root: " + each);
				this.root = each;
				cnt++;
			}
		}
		
		if (cnt != 1) {
			System.out.println("Root region error!");
			assert(false);
		}
		
	}
	
	long getTotalEntryCnt() {
		long ret = 0;
		for (URegion each : dMap.values()) {
			ret += each.cnt;
		}
		return ret;
	}
	
	Set<URegion> getURegionSet() {
		return new HashSet<URegion>(dMap.values());
	}
	
	
	
	public static void main(String args[]) {
		String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/330.art_m";
		//String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/320.equake_m";
		//String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/bots/alignment";
		//String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/bandwidth";
		//String rawDir = "f:\\work\\npb-u\\sp";
		
		//System.out.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";
		
		long start = System.currentTimeMillis();
		SRegionManager sManager = new SRegionManager(new File(sFile), false);
		//sregion.dump();
		URegionManager dManager = new URegionManager(sManager, new File(dFile));
		long end = System.currentTimeMillis();
		System.out.println("Total Entry Cnt = " + dManager.getTotalEntryCnt());
		System.out.printf("Time = %d us\n", end - start);
		/*
		for (URegion each : dManager.getURegionSet()) {
			System.out.println(each);
		}*/
		assert(false);
	}
}

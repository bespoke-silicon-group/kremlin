package pprof;

import java.io.*;
import java.util.*;

public class FeedbackMapper {
	FeedbackMapper(RegionManager manager, String serial, String parallel) {
		this.manager = manager;
		this.convertMap = new HashMap<Integer, Map<Integer, DRegion>>();
		this.serialFeedback = createSerialFeedback(serial);
		//this.parallelFeedback = createParallelFeedback();
	}
	
	class FeedbackEntry {
		int sid, identifier, seq;
		long time;		
		FeedbackEntry(int sid, int id, int seq, long time) {
			this.sid = sid;
			this.identifier = id;
			this.seq = seq;
			this.time = time;
		}
		
		public String toString() {
			String ret = String.format("%d %d %d %d", sid, identifier, seq, time);
			return ret;
		}
	}
	
	Feedback serialFeedback, parallelFeedback;
	RegionManager manager;
	// sid -> map<unique id, dregion>	
	Map<Integer, Map<Integer, DRegion>> convertMap;
	
	
	Feedback getSerialFeedback() {
		return serialFeedback;
	}
	
	Feedback getParallelFeedback() {
		return parallelFeedback;
	}
	
	Map<Integer, List<FeedbackEntry>> readFile(String file) {
		Map<Integer, List<FeedbackEntry>> ret = new HashMap<Integer, List<FeedbackEntry>>();
		try {
			BufferedReader input =  new BufferedReader(new FileReader(file));
			
			while(true) {
				String line = input.readLine();
				if (line == null)
					break;
				
				List<String> list = new ArrayList<String>(); 
				StringTokenizer st = new StringTokenizer(line); 
				while(st.hasMoreElements()) {
					list.add(st.nextToken());
				}
				
				int sregionId = Integer.parseInt(list.get(0));
				int dregionId = Integer.parseInt(list.get(1));
				int seq = Integer.parseInt(list.get(2));
				long time = Long.parseLong(list.get(3));
				
				FeedbackEntry entry = new FeedbackEntry(sregionId, dregionId, seq, time);
				if (!ret.containsKey(sregionId)) {
					ret.put(sregionId, new ArrayList<FeedbackEntry>());
				}
				List<FeedbackEntry> entryList = ret.get(sregionId);
				entryList.add(entry);								
			}			
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
		return ret;
	}
	
	Feedback createSerialFeedback(String serial) {
		Map<Integer, List<FeedbackEntry>> map = readFile(serial);
		Map<DRegion, Long> retMap = new HashMap<DRegion, Long>();		
		
		for (int sid : map.keySet()) {
			Map<Integer, DRegion> newMap = new HashMap<Integer, DRegion>();
			convertMap.put(sid, newMap);
			if (manager.hasSRegion(sid) == false) {
				System.out.println("Error: region id " + sid + " is not registered in pprof");
				assert(false);
			}
			SRegion sregion = manager.getSRegion(sid);			
			List<DRegion> prayList = new ArrayList<DRegion>(manager.getDRegionSet(sregion));
			Collections.sort(prayList);
			/*
			for (DRegion each : set)
				System.out.println(each);
			
			assert(false);*/
			
			List<FeedbackEntry> list = map.get(sid);			
			if (prayList.size() != list.size()) {				
				//System.out.println("SRegion: " + manager.getSRegion(sid));
				//System.out.println("pray size: "+ prayList.size());
				//System.out.println("feedback size: "+ list.size());
				System.out.println("Warning for sid " + sid + " - pray size: " + 
						prayList.size() + " feedback size: " + list.size()); 
				assert(prayList.size() < list.size());
				if (prayList.size() < list.size())
					list = pickTopEntries(list, prayList.size());
					
			}
			assert(prayList.size() >= list.size());
			//for (int i=0; i<prayList.size(); i++) {
			for (int i=0; i<list.size(); i++) {
				DRegion each = prayList.get(i);
				FeedbackEntry entry = list.get(i);
				retMap.put(each, entry.time);
				newMap.put(entry.identifier, each);	
			}
			
		}		
		return new Feedback(this.manager, retMap);
	}
	
	Feedback createParallelFeedback(String parallel) {
		Map<Integer, List<FeedbackEntry>> map = readFile(parallel);
		Map<DRegion, Long> retMap = new HashMap<DRegion, Long>();		
		
		for (int sid : map.keySet()) {			
			List<FeedbackEntry> list = map.get(sid);
			Map<Integer, DRegion> idMap = convertMap.get(sid);
			for (FeedbackEntry each : list) {
				DRegion target = idMap.get(each.identifier);
				retMap.put(target, each.time);
			}			
		}		
		return new Feedback(this.manager, retMap);
	}
	
	/** Dirty hack:
	 * p-ray can drop some dynamic regions
	 * 
	 * @param list
	 * @param n
	 * @return
	 */
	List<FeedbackEntry> pickTopEntries(List<FeedbackEntry> list, int n) {
		List<FeedbackEntry> copied = new ArrayList<FeedbackEntry>(list);
		List<FeedbackEntry> ret = new ArrayList<FeedbackEntry>(list);
		Collections.sort(copied, new FeedbackEntryComparator());
		List<FeedbackEntry> toRemove = copied.subList(n, copied.size());		
		ret.removeAll(toRemove);		
		return ret;
	}
	
	class FeedbackEntryComparator implements Comparator<FeedbackEntry> {
		public int compare(FeedbackEntry item0, FeedbackEntry item1) {
			return (int)(item1.time - item0.time);
		}
	}
}



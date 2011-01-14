package pprof;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.*;

public class Feedback {
	Map<DRegion, Long> map;	// map id <-> exec_time
	RegionManager manager;
	
	public Feedback(RegionManager manager) {
		// empty feedback
		map = new HashMap<DRegion, Long>();
		this.manager = manager;
		assert(manager != null);
		
	}
	
	public Feedback(RegionManager manager, Map<DRegion, Long> map) {		
		this.map = map;
		this.manager = manager;
		assert(manager != null);
		
	}
	
	public Feedback(RegionManager manager, String file) {
		map = new HashMap<DRegion, Long>();
		assert(manager != null);
		this.manager = manager;
		loadRegionFile(file);
	}
	
	void loadRegionFile(String file) {
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
				assert(manager != null);
				if (!manager.hasSRegion(sregionId)) {
					System.out.println("SRegion " + sregionId + " is not registered");
					System.out.println(line);
					assert(false);
				}
				DRegion dregion = manager.getDRegion(sregionId, dregionId);
				
				if (dregion != null)
					map.put(dregion, time);
				/*
				if (!map.keySet().contains(lineStart)) {
					map.put(lineStart, new ArrayList<Long>());					
				}*/
				//List<Long> valueList = map.get(lineStart);
				//valueList.add(time);
			}
			
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	long getExecTime(int sregion, int dregion) {
		DRegion region = manager.getDRegion(sregion, dregion);
		return map.get(region);
	}
	
	long getExecTime(DRegion region) {
		//assert(this.contains(region));
		if (this.contains(region))
			return map.get(region);
		else
			return region.getWork();
	}
	
	boolean contains(DRegion region) {
		return map.keySet().contains(region);
	}
}

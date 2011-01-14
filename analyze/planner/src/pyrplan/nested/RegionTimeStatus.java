package pyrplan.nested;

import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import pprof.*;
import pyrplan.*;

public class RegionTimeStatus {
	SRegionInfoAnalyzer analyzer;
	Map<URegion, Long> timeMap;
	Map<SRegion, RegionRecord> recordMap;
	EntryManager dManager;
	SRegionManager sManager;
	Set<SRegion> parallelSet;
	URegion root;
	
	public RegionTimeStatus(SRegionInfoAnalyzer analyzer) {
		this.analyzer = analyzer;
		this.timeMap = new HashMap<URegion, Long>();
		this.recordMap = new HashMap<SRegion, RegionRecord>();
		this.dManager = analyzer.getDManager();
		this.sManager = analyzer.getSManager();
		this.root = dManager.getRoot();
		for (SRegion each : sManager.getSRegionSet()) {
			for (URegion dregion : dManager.getDEntrySet(each)) {
				this.timeMap.put(dregion, dregion.getWork());
			}			
		}
		this.parallelSet = new HashSet<SRegion>();
	}
	
	public long peekParallelTime(RegionRecord target) {
		return calculateTimeAfterParallelization(target, false);
	}
	
	public void parallelize(RegionRecord target) {
		calculateTimeAfterParallelization(target, true);	
	}
	
	long getExecTime(URegion region) {
		return timeMap.get(region);
	}

	
	long getExecTime() {
		return getExecTime(this.root);
	}
	
	private long calculateTimeAfterParallelization(RegionRecord record, boolean update) {
		SRegion target = record.getRegionInfo().getSRegion();	
		Map<URegion, Long> tempTimeMap = null;
		Map<SRegion, RegionRecord> tempRecordMap = null;
		Set<SRegion> newParallelSet = null;
		
		assert(record.getSpeedup() >= 1.0);
		
		if (update) {
			tempTimeMap = this.timeMap;
			tempRecordMap = this.recordMap;
			newParallelSet = parallelSet;
			
		} else {
			tempTimeMap = new HashMap<URegion, Long>(this.timeMap);
			tempRecordMap = new HashMap<SRegion, RegionRecord>(this.recordMap);
			newParallelSet = new HashSet<SRegion>(parallelSet);			
		}
		
		newParallelSet.add(target);
		tempRecordMap.put(record.getRegionInfo().getSRegion(), record);
		Set<URegion> entrySet = dManager.getDEntrySet(target);		
		List<URegion> workList = new LinkedList<URegion>();
		workList.addAll(entrySet);
		//newSerialSet.addAll(entrySet);		
		
		boolean rootHandled = false;
		while (workList.isEmpty() == false) {
			URegion current = workList.remove(0);
			//System.out.println(current);
			boolean isSerial = true;
			if (newParallelSet.contains(current.getSRegion()))
				isSerial = false;
			
			double speedup = 1.0;
			if (!isSerial)
				speedup = tempRecordMap.get(current.getSRegion()).getSpeedup();
			
			long updatedTime = estimateUEntryTime(current, speedup, tempTimeMap, isSerial);
			tempTimeMap.put(current, updatedTime);		
			if (current.getId() == this.root.getId()) {
				rootHandled = true;
			}
			//System.out.printf("\t\ttime %d serial [%s] after processing %s\n", updatedTime, isSerial, current);
			
			for (URegion parent : current.getParentSet()) {
				if (workList.contains(parent)) {
					workList.remove(parent);					
				} 	
				workList.add(parent);
			}
		}
			
		//assert(rootHandled == true);
		assert(this.root != null);
		assert(tempTimeMap != null);
		long ret = tempTimeMap.get(this.root); 
		
		assert(ret >= this.root.getCriticalPath());
		return ret;
	}
	
	long estimateUEntryTime(URegion entry, double speedup, Map<URegion, Long> tMap, boolean serial) {
		long sum = 0;
		for (URegion child : entry.getChildrenSet()) {
			long cnt = entry.getChildCount(child);
			long time = tMap.get(child);
			sum += cnt * time;
		}
		
		long serialTime = entry.getExclusiveWork() + sum;		
		if (serial)
			return serialTime;	
		
		
		
		long parallelTime = (long)Math.ceil(serialTime / speedup);		
		//return (parallelTime > serialTime) ? serialTime : parallelTime;
		//assert(parallelTime <= serialTime);
		
		if (parallelTime > serialTime) {
			System.out.println("speedup = " + speedup  + " " + entry);
			assert(false);
		}
		return parallelTime;
	}
	
}

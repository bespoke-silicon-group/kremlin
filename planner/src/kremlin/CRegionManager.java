package kremlin;

import java.io.*;
import java.util.*;


/*
 * A class to track all the CRegions (i.e. compressed regions) that were
 * created during kremlin profiling.
 */
public class CRegionManager {
	SRegionManager sManager;	
	CRegion root;
	
	Map<SRegion, Set<CRegion>> cRegionMap = new HashMap<SRegion, Set<CRegion>>();
	
	
	public CRegionManager(SRegionManager sManager, String binFile) {
		this.sManager = sManager;
		readBinaryFile(binFile);
	}	
	
	public Set<CRegion> getCRegionSet(SRegion region) {
		return cRegionMap.get(region); 
	}
	
	void readBinaryFile(String file) {
		Map<Long, Set<Long>> childrenMap = new HashMap<Long, Set<Long>>();
		Map<Long, CRegion> regionMap = new HashMap<Long, CRegion>();

		TraceReader reader = new TraceReader(file);
		Map<CRegion, Long> recursionTarget = new HashMap<CRegion, Long>();
		
				
		// Fill in mapping of unique ids to unique id of child regions.
		for (TraceEntry entry : reader.getTraceList()) {			
			childrenMap.put(entry.uid, entry.childrenSet);			
		}
		
		// Identify recursive CRegions (i.e. CRegionR), setting decendent of
		// all R_INIT nodes to be of type "3" (XXX: what is 3?)
		for (TraceEntry entry : reader.getTraceList()) {
			if (entry.type == 1) {	
				List<Long> list = new ArrayList<Long>();
				list.add(entry.uid);
				
				while (list.isEmpty() == false) {
					TraceEntry current = reader.getEntry(list.remove(0));
					//System.err.printf("\t\tcurrent = id %d type %d\n", current.uid, current.type);
					if (current.type == 0) {						
						current.type = 3;
					}
					
					Set<Long> children = childrenMap.get(current.uid);
					list.addAll(children);					
				}
			}
		}
		
		// Create CRegions, adding them to map of IDs to CRegion objects.
		for (TraceEntry entry: reader.getTraceList()) {
			SRegion sregion = sManager.getSRegion(entry.sid);			
			CallSite callSite = null;
			if (entry.callsiteID != 0)
				callSite = sManager.getCallSite(entry.callsiteID);
			
			
			CRegion region = CRegion.create(sregion, callSite, entry);
			regionMap.put(entry.uid, region);

			// if this links back to a recursive region, add it to the mapping
			// that tracks these "recursive targets"
			if (entry.recursionTarget > 0) {
				recursionTarget.put(region, entry.recursionTarget);
			}
		}
		
		// Set the parent in all CRegions.
		for (long uid : regionMap.keySet()) {
			CRegion region = regionMap.get(uid);			
			assert(region != null);
			Set<Long> children = childrenMap.get(uid);			
			for (long child : children) {
				assert(regionMap.containsKey(child));
				CRegion childRegion = regionMap.get(child);
				childRegion.setParent(region);
				//region.addChild(toAdd);
			}
		}
		
		// Sets recursive target for all CRegions that have one.
		for (CRegion each : recursionTarget.keySet()) {
			//System.err.printf("rec target node id = %d\n", each.id);
			CRegionR region = (CRegionR)each;
			CRegion target = regionMap.get(recursionTarget.get(each));
			assert(target.getRegionType() == CRecursiveType.REC_INIT);
			region.setRecursionTarget(target);
		}
		
		// Create mappings from SRegion objects to associated CRegion objects.
		// We'll also find and mark the root CRegion.
		for (CRegion region : regionMap.values()) {
			if (region.getParent() == null) {
				//assert(this.root == null);
				this.root = region;
			}			
			SRegion sregion = region.getSRegion();
			if (!cRegionMap.keySet().contains(sregion))
				cRegionMap.put(sregion, new HashSet<CRegion>());
			
			Set<CRegion> set = cRegionMap.get(sregion);
			set.add(region);			
		}
		
		calculateRecursiveWeight(new HashSet(regionMap.values()));
	}
	
	/*
	 * Calculate the "recursive weight" for each CRegion in the given set.
	 */
	private void calculateRecursiveWeight(Set<CRegion> set) {		
		//Set<CRegion> sinkSet = new HashSet<CRegion>();
		Map<CRegion, Set<CRegion>> map = new HashMap<CRegion, Set<CRegion>>();		
		
		// Step 1: Add map entries for all R_INIT nodes
		for (CRegion each : set) {			
			if (each.getRegionType() == CRecursiveType.REC_INIT) {				
				map.put(each,  new HashSet<CRegion>());		
				//System.err.printf("\nadding node %d to init map\n", each.id);
				//System.err.printf("\t%s\n", each.getRegionType());
				assert(each instanceof CRegionR);
			}
		}
		
		// Step 2: Create mapping between R_INIT CRegion and all its associated
		// R_SINK CRegions
		for (CRegion each : set) {
			if (each.getRegionType() == CRecursiveType.REC_SINK) {
				CRegion target = ((CRegionR)each).getRecursionTarget();
				Set<CRegion> sinkSet = map.get(target);				
				sinkSet.add(each);
				//System.err.printf("\nadding node %d to sink \n", each.id);
			}
		}
		
		// Step 3: Set the recursive weight for all R_INIT CRegions.
		for (CRegion each : map.keySet()) {
			setRecursionWeight((CRegionR)each, map.get(each));
		}
	}
	
	void setRecursionWeight(CRegionR init, Set<CRegion> sinks) {
		int maxSize = 0;

		// Step 1: Find the maximum number of stats in any one R_SINK region
		for (CRegion each : sinks) {
			CRegionR current = (CRegionR)each;
			if (current.getStatSize() > maxSize)
				maxSize = current.getStatSize();
		}
		//System.err.printf("init = %d, sinks size = %d maxSize = %d\n", init.id, sinks.size(), maxSize);

		for (int depth=0; depth<maxSize; depth++) {
			Map<CRegion, Long> map = new HashMap<CRegion, Long>();

			// For all CRegions that are descendents of init's parent but are
			// ancestors of a sink, fill in the mapping from that CRegion to
			// the total work (XXX: this is SERIOUSLY confusing business...)
			for (CRegion s : sinks) {
				CRegionR sink = (CRegionR) s;
				if (sink.getStatSize() <= depth)
					continue;
				
				CRegion target = sink.getParent();			
				
				while (target != init.getParent()) {
					if (!map.containsKey(target)) {
						map.put(target, (long)0);					
					}
					long prev = map.get(target);
					long updated = prev + sink.getStat(depth).getTotalWork();
					map.put(target, updated);
					//System.err.printf("updating value to %d at %d\n", updated, target.id);
					target = target.getParent();
				}
			}			
			
			// For all the CRegions in our mapping, set the recusive weight at
			// the current depth.
			// This recursive weight is the ratio of init's total work to the
			// region's total work.
			long totalWork = map.get(init);
			for (CRegion each : map.keySet()) {
				long work = map.get(each);
				double weight = work / (double)totalWork;
				//System.err.printf("Setting weight of node %d at depth %d to %.2f\n", each.id, depth, weight);
				((CRegionR)each).getStat(depth).setRecursionWeight(weight);
			}
		}		
	}
	
	public CRegion getRoot() { return this.root; }	
	
	public Set<CRegion> getLeafSet() {
		Set<CRegion> ret = new HashSet<CRegion>();		
		for (Set<CRegion> each : cRegionMap.values()) {
			for (CRegion region : each)
				if (region.isLeaf())
					ret.add(region);
		}
		return ret;
	}
	
	public Set<CRegion> getCRegionSet() {
		Set<CRegion> ret = new HashSet<CRegion>();
		for (SRegion key : cRegionMap.keySet()) {
			ret.addAll(cRegionMap.get(key));			
		}
		return ret;
	}
	
	public Set<CRegion> getCRegionSet(double threshold) {
		Set<CRegion> ret = new HashSet<CRegion>();
		for (SRegion key : cRegionMap.keySet()) {
			for (CRegion each : cRegionMap.get(key)) {
				if (this.getTimeReduction(each) > threshold)				
					ret.add(each);
			}			
		}
		return ret;
	}
	
	public void printStatistics() {
		int cntTotal = 0;
		int cntLoop = 0;
		int cntFunc = 0;
		int cntBody = 0;
		
		for (SRegion each : cRegionMap.keySet()) {
			//System.out.println(each);
			Set<CRegion> set = cRegionMap.get(each);
			cntTotal += set.size();
			
			if (each.getType() == RegionType.LOOP)
				cntLoop += set.size();
			else if (each.getType() == RegionType.FUNC){				
				cntFunc += set.size();
			} else 
				cntBody += set.size();
		}
		
		System.out.printf("Region Count (Total / Loop / Func / Body) = %d / %d / %d / %d\n", 
				cntTotal, cntLoop, cntFunc, cntBody);		
	}
	
	public double getCoverage(CRegion region) {
		double coverage = ((double)region.getTotalWork() / getRoot().getTotalWork()) * 100.0;
		return coverage;
	}
	
	public double getTimeReduction(CRegion region) {
		return getCoverage(region) * (1.0 - 1.0 / region.getSelfP());
	}
}

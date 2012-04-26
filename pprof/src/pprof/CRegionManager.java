package pprof;

import java.io.*;
import java.util.*;


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
		
				
		// log parent, children relationship
		for (TraceEntry entry : reader.getTraceList()) {			
			childrenMap.put(entry.uid, entry.childrenSet);			
		}
		
		// identify CRegionR nodes
		for (TraceEntry entry : reader.getTraceList()) {
			//System.err.printf("testing node with id %d with type %d\n", entry.uid, entry.type);
			if (entry.type == 1) {	
				//System.err.printf("\tdetected a RInit node with id %d\n", entry.uid);
				// if a node is RInit type, set its descendant nodes to type 3
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
		
		for (TraceEntry entry: reader.getTraceList()) {
			SRegion sregion = sManager.getSRegion(entry.sid);			
			CallSite callSite = null;
			if (entry.callSiteValue != 0)
				callSite = sManager.getCallSite(entry.callSiteValue);
			
			
			CRegion region = CRegion.create(sregion, callSite, entry);
			regionMap.put(entry.uid, region);
			if (entry.recursionTarget > 0) {
				recursionTarget.put(region, entry.recursionTarget);
			}			
		}		
		
		// connect parent with children
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
		
		for (CRegion each : recursionTarget.keySet()) {
			System.err.printf("rec target node id = %d\n", each.id);
			CRegionR region = (CRegionR)each;
			CRegion target = regionMap.get(recursionTarget.get(each));
			assert(target.getRegionType() == CRecursiveType.REC_INIT);
			region.setRecursionTarget(target);
		}
		
		for (CRegion region : regionMap.values()) {
			if (region.getParent() == null) {
				//System.out.println("no parent: " + region);
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
	
	private void calculateRecursiveWeight(Set<CRegion> set) {		
		//Set<CRegion> sinkSet = new HashSet<CRegion>();
		Map<CRegion, Set<CRegion>> map = new HashMap<CRegion, Set<CRegion>>();		
		
		for (CRegion each : set) {			
			if (each.getRegionType() == CRecursiveType.REC_INIT) {				
				map.put(each,  new HashSet<CRegion>());		
				System.err.printf("\nadding node %d to init map\n", each.id);
				//System.err.printf("\t%s\n", each.getRegionType());
				assert(each instanceof CRegionR);
			}
		}
		
		for (CRegion each : set) {
			if (each.getRegionType() == CRecursiveType.REC_SINK) {
				CRegion target = ((CRegionR)each).getRecursionTarget();
				Set<CRegion> sinkSet = map.get(target);				
				sinkSet.add(each);
				System.err.printf("\nadding node %d to sink \n", each.id);
			}
		}
		
		for (CRegion each : map.keySet()) {
			setRecursionWeight((CRegionR)each, map.get(each));
		}
	}
	
	void setRecursionWeight(CRegionR init, Set<CRegion> sinks) {
		
		int maxSize = 0;
		for (CRegion each : sinks) {
			CRegionR current = (CRegionR)each;
			if (current.getStatSize() > maxSize)
				maxSize = current.getStatSize();
		}
		System.err.printf("init = %d, sinks size = %d maxSize = %d\n", init.id, sinks.size(), maxSize);
		for (int depth=0; depth<maxSize; depth++) {
			Map<CRegion, Long> map = new HashMap<CRegion, Long>();
			for (CRegion each : sinks) {
				CRegionR source = (CRegionR) each;
				if (source.getStatSize() <= depth)
					continue;
				
				CRegion target = source.getParent();			
				
				while (target != init.getParent()) {
					if (!map.containsKey(target)) {
						map.put(target, (long)0);					
					}
					long prev = map.get(target);
					long updated = prev + source.getStat(depth).getTotalWork();
					map.put(target, updated);
					System.err.printf("updating value to %d at %d\n", updated, target.id);
					target = target.getParent();
				}
			}			
			
			// weight = rWork(init) / rWork(node)
			long totalWork = map.get(init);
			for (CRegion each : map.keySet()) {
				long work = map.get(each);
				double weight = work / (double)totalWork;
				System.err.printf("Setting weight of node %d at depth %d to %.2f\n", each.id, depth, weight);
				((CRegionR)each).getStat(depth).setRecursionWeight(weight);
				
			}
		}		
	}
	
	public CRegion getRoot() {
		return this.root;
	}
	
	public Set<CRegion> getLeafSet() {
		Set<CRegion> ret = new HashSet<CRegion>();
		for (SRegion key : cRegionMap.keySet()) {
			for (CRegion each : cRegionMap.get(key)) {
				if (each.getChildrenSet().size() == 0)
					ret.add(each);
			}						
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
	

	
	List<Long> cloneContext(List<Long> in) {
		List<Long> ret = new ArrayList<Long>(in);
		return ret;		
	}
	/*
	void buildURegionSet() {
		System.out.println("Total URegion Size = " + uManager.getURegionSet().size());
		int cnt = 0;
		List<URegion> workList = new ArrayList<URegion>();
		Set<URegion> retired = new HashSet<URegion>();
		workList.add(uManager.getRoot());		
		
		List<Long> context = new ArrayList<Long>();
		context.add(uManager.getRoot().callSite);
		
		while(!workList.isEmpty()) {
			URegion current = workList.remove(0);
			cnt++;
			
			if (cnt % 10000 == 0) {
				System.out.println("~");
			}
			context.add(current.callSite);
			
			SRegion sregion = current.getSRegion();
			Set<URegionSet> set = null;
			
			if (map.containsKey(sregion))
				set = map.get(current.getSRegion());
			else {
				set = new HashSet<URegionSet>();
				map.put(sregion, set);
			}
			
			boolean found = false;
			for (URegionSet candidate : set) {
				if (isContextSame(context, candidate.context)) {
					candidate.add(current);
					found = true;
				}
			}
			// new entry
			if (!found) {
				System.out.println(context);
				URegionSet toAdd = new URegionSet(current.getSRegion(), cloneContext(context));
				toAdd.add(current);
				set.add(toAdd);
			}
			
			context.remove(context.size()-1);
			
			for (URegion child : current.getChildrenSet())
				workList.add(child);			
		}		
	}*/
	/*
	void buildURegionSet() {
		System.out.println("Total URegion Size = " + uManager.getURegionSet().size());
		int cnt = 0;
		for (URegion each : uManager.getURegionSet()) {			
			cnt++;
			if (cnt % 10000 == 0) {
				System.out.print("~");
			}
				
			SRegion sregion = each.getSRegion();
			Set<URegionSet> set = null;
			
			if (map.containsKey(sregion))
				set = map.get(each.getSRegion());
			else {
				set = new HashSet<URegionSet>();
				map.put(sregion, set);
			}
			
			List<Long> context = each.getCallStack();
			
			boolean found = false;
			for (URegionSet candidate : set) {
				if (isContextSame(context, candidate.context)) {
					candidate.add(each);
					found = true;
				}
			}
			// new entry
			if (!found) {
				URegionSet toAdd = new URegionSet(each.getSRegion(), context);
				toAdd.add(each);
				set.add(toAdd);
			}
		}
	}*/
	/*
	void buildURegionSet() {
		System.out.println("Total URegion Size = " + uManager.getURegionSet().size());
		int cnt = 0;
		
		for (SRegion sregion : uManager.getSRegionManager().getSRegionSet()) {
			Set<URegionSet> set = null;			
			set = new HashSet<URegionSet>();
			map.put(sregion, set);
			
			for (URegion uregion : uManager.getDEntrySet(sregion)) {
				cnt++;
				if (cnt % 10000 == 0) {
					System.out.print("~");
				}
				List<Long> context = uregion.getCallStack();
				boolean found = false;
				for (URegionSet candidate : set) {
					if (isContextSame(context, candidate.context)) {
						candidate.add(uregion);
						found = true;
					}
				}
				if (!found) {
					URegionSet toAdd = new URegionSet(sregion, context);
					toAdd.add(uregion);
					set.add(toAdd);
				}
			}
		}
	}*/
	
	boolean isContextSame(List<Long> src, List<Long> target) {
		if (src.size() != target.size())
			return false;
		
		for (int i=0; i<src.size(); i++) {
			if (src.get(i) != target.get(i))
				return false;
		}
		return true;
	}
	/*
	public void dump() {
		CRegionPrinter printer = new CRegionPrinter(this);
		for (SRegion each : cRegionMap.keySet()) {
			//System.out.println(each);
			Set<CRegion> set = cRegionMap.get(each);
			for (CRegion cregion : set) {
				//System.out.println("\t" + cregion);
				System.out.println(printer.getString(cregion) + "\n");
			}
		}
	}*/
	
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
	
	public static void main(String args[]) {
		System.out.println("CRegionManager");
		//String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test/CINT2000/175.vpr/src";
		String dir = "g:\\work\\ktest\\recursion";
		//String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test/loop";
		String sFile = String.format("%s/sregions.txt", dir);
		String bFile = String.format("%s/kremlin.bin", dir);
		SRegionManager sManager = new SRegionManager(new File(sFile), true);
		CRegionManager cManager = new CRegionManager(sManager, bFile);
	}
}

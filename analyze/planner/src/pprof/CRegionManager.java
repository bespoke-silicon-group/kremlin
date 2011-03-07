package pprof;

import java.io.*;
import java.util.*;

import pprof.URegionManager.Entry;

public class CRegionManager {
	SRegionManager sManager;
	URegionManager uManager;
	CRegion root;
	Map<SRegion, Set<URegionSet>> map;
	Map<SRegion, Set<CRegion>> cRegionMap = new HashMap<SRegion, Set<CRegion>>();
	
	class URegionSet {
		URegionSet(SRegion region, List<Long> context) {
			this.sregion = region;
			this.context = context;
			this.set = new HashSet<URegion>();
		}
		
		SRegion sregion;
		List<Long> context;
		Set<URegion> set;
		
		void add(URegion toAdd) {
			assert(toAdd.getSRegion() == this.sregion);
			set.add(toAdd);
		}		
	}
	
	public CRegionManager(SRegionManager sManager, String binFile) {
		this.sManager = sManager;
		readBinaryFile(binFile);
	}
	
	public CRegionManager(URegionManager uManager) {		
		this.map = new HashMap<SRegion, Set<URegionSet>>();
		this.uManager = uManager;
		System.err.println("start bulding URegionSet");
		buildURegionSet();
		System.err.println("start bulding URegion Tree");
		buildCRegionTree();
		System.err.println("start connecting");
		connect();
		System.err.println("done");
	}
	
	public Set<CRegion> getCRegionSet(SRegion region) {
		return cRegionMap.get(region); 
	}
	
	void readBinaryFile(String file) {
		Map<Long, Set<Long>> childrenMap = new HashMap<Long, Set<Long>>();
		Map<Long, CRegion> regionMap = new HashMap<Long, CRegion>();
		
		try {
			DataInputStream input =  new DataInputStream(new FileInputStream(file));			
		
			while(true) {
				Set<Long> childrenSet = new HashSet<Long>();
				//Map<Long, Long> childrenMap = new HashMap<Long, Long>();
				long uid = Long.reverseBytes(input.readLong());
				long sid = Long.reverseBytes(input.readLong());
				long callSiteValue = Long.reverseBytes(input.readLong());
				long cnt = Long.reverseBytes(input.readLong());
				long work = Long.reverseBytes(input.readLong());				
				long tpWork = Long.reverseBytes(input.readLong());
				long spWork = Long.reverseBytes(input.readLong());
				
				double minSP = (Long.reverseBytes(input.readLong())) / 100.0;
				double maxSP = (Long.reverseBytes(input.readLong())) / 100.0;
				long readCnt = Long.reverseBytes(input.readLong());
				long writeCnt = Long.reverseBytes(input.readLong());
				
								
				long nChildren = Long.reverseBytes(input.readLong());
				//assert(cp != 0);

				for (int i=0; i<nChildren; i++) {
					long childUid = Long.reverseBytes(input.readLong());
					childrenSet.add(childUid);
				}
				SRegion sregion = sManager.getSRegion(sid);
				
				//System.out.println("\t" + childrenSet);
				CallSite callSite = null;
				if (callSiteValue != 0)
					callSite = sManager.getCallSite(callSiteValue);
				
				//System.out.printf("[%d %d]*%d\t %d %d %d %d %d children: %d %s\n",
				//		uid, callSiteValue, cnt, work, tpWork, spWork, readCnt, writeCnt, nChildren, sregion);
				
				CRegion region = new CRegion(sregion, uid, callSite, cnt, work, tpWork, spWork, minSP, maxSP, readCnt, writeCnt);
				regionMap.put(uid, region);
				childrenMap.put(uid, childrenSet);
				//uMap.put(uid, new Entry(uid, sid, work, cp, callSite, readCnt, writeCnt, readLineCnt, writeLineCnt, cnt, childrenMap));
				/*
				if (nChildren == 0) {
					workList.add(0, uMap.get(uid));				
				} else {
					workList.add(uMap.get(uid));
				}*/
			}			
			
		} catch(Exception e) {
			//System.out.println(e);
			if (e instanceof java.io.EOFException == false) {
				e.printStackTrace();
				assert(false);
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
	
	void buildCRegionTree() {
		long totalWork = uManager.getRoot().getWork();
		for (SRegion each : map.keySet()) {
			Set<URegionSet> set = map.get(each);
			Set<CRegion> toAddSet = new HashSet<CRegion>();
			cRegionMap.put(each, toAddSet);
			for (URegionSet target : set) {
				CRegion toAdd = new CRegion(each, target.set, target.context, totalWork);
				toAddSet.add(toAdd);
			}
		}		
	}
	
	
	
	void connect() {
		
		System.out.println("Total CRegion Size = " + this.getCRegionSet().size());
		for (SRegion sregion : cRegionMap.keySet()) {
			Set<CRegion> set = cRegionMap.get(sregion);
			for (CRegion cregion : set) {
				SRegion parent = cregion.getParentSRegion();
				if (parent == null) {
					this.root = cregion;
					continue;
				}
				
				Set<CRegion> targetSet = cRegionMap.get(parent);
				
				for (CRegion each : targetSet) {
					if (isParent(cregion, each)) {
						cregion.setParent(each);
						break;
					}
				}
			}			
		}
	}
	
	boolean isParent(CRegion child, CRegion parent) {
		List<Long> childContext = child.getContext();
		List<Long> parentContext = parent.getContext();	
		//System.out.println("parent: " + parent.getSRegion() + "\t" + parentContext);
		//System.out.println("child: " + child.getSRegion() + "\t" + childContext);
		
		if (parentContext.size() +1 != childContext.size()) {
			//System.out.println("false1");
			return false;
		}
		
		for (int i=0; i<parentContext.size(); i++) {
			long childValue = childContext.get(i+1);
			long parentValue = parentContext.get(i);
			if (childValue != parentValue) {				
				//System.out.println("false2: " + i + " " + childContext.get(i+1) + " " + parentContext.get(i));	
				return false;
			}
		}		
		return true;
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
	}
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
	}
	
	public double getCoverage(CRegion region) {
		double coverage = ((double)region.getTotalWork() / getRoot().getTotalWork()) * 100.0;
		return coverage;
	}
	
	public double getTimeReduction(CRegion region) {
		return getCoverage(region) * (1.0 - 1.0 / region.selfParallelism);
	}
	
	public static void main(String args[]) {
		System.out.println("CRegionManager");
		//String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test/CINT2000/175.vpr/src";
		String dir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test/loop";
		String sFile = String.format("%s/sregions.txt", dir);
		String bFile = String.format("%s/kremlin.bin", dir);
		SRegionManager sManager = new SRegionManager(new File(sFile), true);
		CRegionManager cManager = new CRegionManager(sManager, bFile);
	}
}

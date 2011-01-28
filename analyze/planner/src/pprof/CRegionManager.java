package pprof;

import java.util.*;

public class CRegionManager {
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
	
	public CRegion getRoot() {
		return this.root;
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
		for (SRegion each : cRegionMap.keySet()) {
			System.out.println(each);
			Set<CRegion> set = cRegionMap.get(each);
			for (CRegion cregion : set) {
				System.out.println("\t" + cregion);
			}
		}
	}
	
}

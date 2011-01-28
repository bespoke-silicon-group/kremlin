package pprof;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.*;


public class URegion {
	static long num = 0;
	static final long SYNC_COST = 30000; 
	URegion(SRegion sregion, long uid, long work, long cp, long callSite, 
			long readCnt, long writeCnt, long readLineCnt, long writeLineCnt, 
			long cnt, Map<URegion, Long> map) {
		assert(sregion != null);
		//this.id = num++;
		this.id = uid;
		this.sregion = sregion;
		this.work = work;
		this.cnt = cnt;
		this.callSite = callSite;
		this.readCnt = readCnt;
		this.writeCnt = writeCnt;
		this.readLineCnt = readLineCnt;
		this.writeLineCnt = writeLineCnt;
		this.cp = cp;		
		
		if (Util.isReduction(sregion)) {
			this.cp += SYNC_COST;
			this.work += SYNC_COST;
		}
		
		this.parentSet = new HashSet<URegion>();		
		this.children = new HashMap<URegion, Long>(map);
		this.numChildren = map.keySet().size();
		calcExclusiveWork();
		
		if (cp == 0 || work == 0) {
			System.out.println("Warning: cp or work is zero");
			System.out.println("\t" + this);
		}
		
		for (URegion each : map.keySet()) {
			each.addParent(this);
		}		
		
		//assert(readCnt >= readLineCnt);
		/*
		if (readCnt > readLineCnt * 17) {
			System.out.printf("%s\tread num (%d, %d, %.2fX)\n", 
					this.sregion, this.readCnt, this.readLineCnt,
					(double)this.readCnt / this.readLineCnt);
		}*/
		//assert(readCnt <= readLineCnt * 17);
		
	}
	boolean isReduction;
	long id;
	SRegion sregion;
	long work;
	long cp;
	long callSite;
	long readCnt;
	long writeCnt;
	long readLineCnt;
	long writeLineCnt;
	long cnt;
	long exclusiveWork;
	long numChildren;
	double pFactor;
	Map<URegion, Long> children;
	Set<URegion> parentSet;
	//List<Long> callStack = new ArrayList<Long>();
	//DEntry parent;
	//Set<DEntry> childrenSet;
	//Map<DEntry, Long> childrenCntMap;
	//Map<Long, List<Long>> 
	
	public List<Long> getCallStack() {
		List<Long> ret = new ArrayList<Long>();
		URegion current = this;
		while (current != null) {
			long callSite = current.callSite;
			ret.add(callSite);
			if (current.getParentSet().size() == 0)
				current = null;
			else {
				current = current.getParentSet().iterator().next();
				if (current.getParentSet().size() != 1) {					
					for (URegion parent : current.getParentSet()) {
						assert(parent.callSite == current.callSite);
					}
					//System.out.println(current);
				}
				//assert(current.getParentSet().size() == 1);
				
			}				
		}
		return ret;
	}
	
	public long getId() {
		return id;
	}
	
	public long getWork() {
		return work;
	}
	
	public Set<URegion> getParentSet() {
		return parentSet;
	}
	
	public SRegion getSRegion() {
		return sregion;
	}
	
	String toChildrenSRegionSetString() {
		StringBuffer buffer = new StringBuffer();
		for (URegion each : children.keySet()) {
			buffer.append(each.id + "\t");
		}
		return buffer.toString();			
	}
	
	public long getCriticalPath() {
		return this.cp;
	}
	
	long getChildrenCount() {
		long sum = 0;
		for (URegion each : children.keySet()) {
			sum += children.get(each);
		}
		return sum;
	}
	
	private void calcExclusiveWork() {
		long ret = this.work;
		for (URegion entry : this.children.keySet()) {
			ret -= this.children.get(entry) * entry.work;
		}		
		this.exclusiveWork = ret;
	}
	
	public long getExclusiveWork() {
		return this.exclusiveWork;
	}
	
	boolean isCompatible(URegion target) {
		long nChildren = target.children.keySet().size();
		if (target.callSite != this.callSite)
			return false;
		
		if (target.readCnt != this.readCnt)
			return false;
		
		if (target.writeCnt != this.writeCnt)
			return false;
		
		if (target.readLineCnt != this.readLineCnt)
			return false;
		
		if (target.writeLineCnt != this.writeLineCnt)
			return false;
		
		if (target.cp != this.cp)
			return false;		
		
		if (nChildren != numChildren)
			return false;
		
		if (target.children.keySet().size() != this.children.keySet().size())
			return false;
		
		for (URegion each : target.children.keySet()) {
			if (children.containsKey(each) == false) {
				return false;
			}
			
			if (target.children.get(each) != children.get(each))
				return false;
		}
		
		return true;
	}

	
	void addParent(URegion entry) {
		parentSet.add(entry);
	}
	
	public String toString() {
		return String.format("%s id %d, work (%d, %d), cp %d, mem (%d, %d), cnt %d, parallelism %.2f, %.2f", 
				sregion, id, work, exclusiveWork, cp, readCnt, writeCnt, cnt, getParallelism(), getSelfParallelism());
	}
	
	public double getParallelism() {
		return ((double)work) / cp;
	}
	
	public double getSelfParallelism() {
		URegion entry = this;		
		
		if (entry.getChildrenSet().size() == 0) {
			if (entry.cp < 1.0)
				return 1.0;
			return entry.work / (double)entry.cp;
		}
		
		long serial = 0;
		long childrenTotal = 0;
		for (URegion child : entry.getChildrenSet()) {
			long cnt = entry.getChildCount(child);
			serial += child.cp * cnt;			
			//assert(child.cp > 0);
			childrenTotal += child.work * cnt;
		}
		long exclusive = entry.work - childrenTotal;
		serial += exclusive;		
		
		if (serial < entry.cp) {
			System.out.printf("serial = %d cp = %d\n", serial, entry.cp);
			System.out.println(entry.getSRegion());
			assert(false);
		}
		assert(serial >= entry.cp);
		return  serial / (double)entry.cp;
	}
	
	public Long getChildCount(URegion child) {
		if (this.children.containsKey(child))
			return this.children.get(child);
		else
			return 0L;
	}
	
	public Set<URegion> getChildrenSet() {
		return this.children.keySet();
	}
	
	public void dumpChildren() {
		System.out.printf("%s\n", this);
		/*
		System.out.printf("# of children: %d\n", this.childrenSet.size());
		for (DEntry child : this.childrenSet) {
			assert(child.parentSet.contains(this));
			System.out.printf("\t%d: %s\n", this.childrenCntMap.get(child), child);
		}*/
	}
}

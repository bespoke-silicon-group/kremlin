package pprof;
import pprof.*;

import java.util.*;

public class SRegionInfoGroup implements Comparable {
	List<SRegionInfo> list; //from outer region to inner region
	List<SRegionInfoGroup> children;
	List<SRegionInfoGroup> parents;
	
	double maxSpeedup = 1.0;
	boolean groupParallelized;
	
	public SRegionInfoGroup(List<SRegionInfo> list) {
		this.list = new ArrayList<SRegionInfo>(list);
		this.groupParallelized = false;
		
		for (SRegionInfo each : list) {
			if (each.getSelfSpeedup() > maxSpeedup)
				maxSpeedup = each.getSelfSpeedup();
			
			if (each.isExploited())
				this.groupParallelized = true;
		}
		
		parents = new ArrayList<SRegionInfoGroup>();
		children = new ArrayList<SRegionInfoGroup>();
	}
	
	public int getChildrenSize() {
		return children.size();
	}
	
	public int getParentSize() {
		return parents.size();
	}
	
	public Set<SRegionInfoGroup> getParentSet() {
		return new HashSet<SRegionInfoGroup>(parents);		
	}
	
	public Set<SRegionInfoGroup> getChildrenSet() {
		return new HashSet<SRegionInfoGroup>(children);		
	}
	
	public SRegionInfoGroup getParent(int index) {
		assert(parents.size() > index);
		return parents.get(index);
	}
	
	public SRegionInfoGroup getChild(int index) {
		assert(children.size() > index);
		return children.get(index);
	}
	
	public int size() {
		return list.size();
	}
	
	public boolean isGroupParallelized() {
		return this.groupParallelized;
	}
	
	public String toString() {
		StringBuffer buf = new StringBuffer();
		for (SRegionInfo entry : list) {
			buf.append(entry + "\n");
		}
		return buf.toString();
	}
	
	public SRegionInfo getEntry(int i) {
		return list.get(i);
	}	 
	
	@Override
	public int compareTo(Object o) {
		SRegionInfoGroup target = (SRegionInfoGroup)o;		
		if (target.maxSpeedup - this.maxSpeedup > 0)
			return 1;
		else
			return -1;		
	}
}

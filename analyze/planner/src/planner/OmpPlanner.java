package planner;
import pprof.*;

import java.util.*;

public class OmpPlanner {
	SRegionInfoAnalyzer anal;
	SRegionInfoGroupManager manager;
	Map<SRegionInfoGroup, OmpUnit> map;
	
	public OmpPlanner(SRegionInfoAnalyzer anal) {
		this.anal = anal;
		this.map = new HashMap<SRegionInfoGroup, OmpUnit>();
		
	}
		
	
	List<SRegionInfo> getRecList() {
		List<SRegionInfo> ret = new ArrayList<SRegionInfo>();
		return ret;
	}
	
	void build() {
		SRegionInfo root = anal.getRootInfo();
		SRegionInfoGroup rootGroup = manager.getGroup(root);
		
		List<SRegionInfoGroup> list = new ArrayList<SRegionInfoGroup>();
		list.add(rootGroup);
		
		
		
		while(list.isEmpty() == false) {
			SRegionInfoGroup current = list.remove(0);
			processGroup(current);
			for (int i=0; i<current.getChildrenSize(); i++) {
				list.add(current.getChild(i));
			}
		}
	}
	
	OmpUnit getUnit(SRegionInfoGroup target) {
		assert(map.containsKey(target));
		return map.get(target);
	}
	
	void processGroup(SRegionInfoGroup target) {
		OmpUnit targetUnit = getUnit(target);
		int nParents = target.getParentSize();
		int nChildren = target.getChildrenSize();
		
		// compare if the new node speedup against existing ones					
		Set<OmpUnit> parentSet = new HashSet<OmpUnit>();
		for (int i=0; i<nParents; i++) {
			parentSet.addAll(targetUnit.getSelectedSet());
		}
		double parentSetSpeedup = calculateSpeedup(parentSet);
		double selfSpeedup = calculateSpeedup(targetUnit);
		
		if (selfSpeedup > parentSetSpeedup) {
			for (OmpUnit parent : parentSet) {
				parent.removeSelection();
			}
			targetUnit.addSelection(targetUnit);
			
			// if it is changed,
			// update previous nodes
			// where to update?
			
		} else {
			targetUnit.addSelection(parentSet);
		}
		
		
		
		// multiple child?
		//if (nChildren > 1) {
		//	
		//}
	}
	
	// update parents of group
	void update(SRegionInfoGroup child, Set<OmpUnit> removeSet, Set<OmpUnit> addSet) {
		List<SRegionInfoGroup> list = new ArrayList<SRegionInfoGroup>();
		
		for (int i=0; i<child.size(); i++) {
			SRegionInfoGroup parent = child.getParent(i);
			list.add(parent);			
		}
		
		while(list.isEmpty() == false) {
			SRegionInfoGroup target = list.remove(0);
			//target.updateChildSpeedup(child, removeSet, addSet);
			
			list.addAll(target.getParentSet());
		}
	}
	
	double calculateSpeedup(Set<OmpUnit> unitSet) {
		return 1.0;
	}
	
	double calculateSpeedup(OmpUnit unit) {
		return 1.0;
	}
}

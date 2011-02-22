package planner;

import pprof.*;
import java.util.*;

public class OmpUnit {
	SRegionInfoGroup group;
	Map<SRegionInfoGroup, Double> childrenMap;
	//boolean isSelected;
	Set<OmpUnit> selectedSet;
	double speedupPercent;
	
	OmpUnit(SRegionInfoGroup group) {
		this.group = group;		
		this.selectedSet = new HashSet<OmpUnit>();		
		this.speedupPercent = getSpeedupPercent();
		
	}
	
	private double getSpeedupPercent() {
		double max = 0;
		for (int i=0; i<group.size(); i++) {
			SRegionInfo info = group.getEntry(i);
			if (info.getSelfSpeedup() > max)
				max = info.getSelfSpeedup();
		}
		return (max - 1.0) * 100.0;
	}
	
	void addSelection(OmpUnit unit) {
		//this.isSelected = true;
		this.selectedSet.add(unit);		
	}	
	
	void addSelection(Set<OmpUnit> set) {
		//this.isSelected = true;
		this.selectedSet.addAll(set);		
	}	
	
	void removeSelection() {
		this.selectedSet = new HashSet<OmpUnit>();
	}
	
	Set<OmpUnit> getSelectedSet() {
		return this.selectedSet;
	}
}

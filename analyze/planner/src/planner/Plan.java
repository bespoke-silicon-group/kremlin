package planner;

import java.util.*;

public class Plan {
	double totalReduction;
	Target target;
	List<CRegionRecord> list;
	
	Plan(List<CRegionRecord> list, Target target, double totalReduction) {
		this.list = list;
		this.target = target;
		this.totalReduction = totalReduction;
	}	
	
	public double getTimeReduction() {
		return this.totalReduction;
	}
	
	public List<CRegionRecord> getCRegionList() {
		return this.list;
	}
	
	public Target getTarget() {
		return this.target;
	}
}

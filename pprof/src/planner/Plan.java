package planner;

import java.util.*;

public class Plan {
	double totalReduction;
	double serialTime;
	Target target;
	List<CRegionRecord> list;
	
	Plan(List<CRegionRecord> list, Target target, double totalReduction, double serialTime) {
		this.list = list;
		this.target = target;
		this.totalReduction = totalReduction;
		this.serialTime = serialTime;
	}
	public double getParallelTime() {
		return serialTime * 0.01 * (100.0 - getTimeReduction());
	}
	public double getSerialTime() {
		return this.serialTime;
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

package planner;

import java.util.*;

public enum PlannerType {
	Query,
	Profiler,
	Cilk,
	GPU,
	OpenMP;
	
	private static final Map<String, PlannerType> stringToEnum
		= new HashMap<String, PlannerType>();
	
	static {
		for (PlannerType each : values()) {
			stringToEnum.put(each.toString().toLowerCase(), each);
		}
	}
	
	public static PlannerType fromString(String str) {
		return stringToEnum.get(str.toLowerCase());
	}
}

package pyrplan;

import java.util.*;

import pprof.SRegion;

public class RecList {
	
	public class RecUnit {
		SRegion region;
		long beforeTime;
		long afterTime;
		double speedup;
		
		public RecUnit(SRegion region, long before, long after) {
			this.region = region;
			this.beforeTime = before;
			this.afterTime = after;
			this.speedup = before / (double)after; 
			
		}
		
		public SRegion getRegion() {
			return region;
		}
		
		public double getSpeedup() {
			return speedup;
		}
	}
	
	public int size() {
		return list.size();
	}
	
	public RecUnit get(int index) {
		return list.get(index);
	}
	
	
	
	public RecList() {
		this.list = new ArrayList<RecUnit>();
	}
	
	List<RecUnit> list;
	
	public void addRecUnit(SRegion region, long before, long after) {
		list.add(0, new RecUnit(region, before, after));
	}
	
	public void appendRecUnit(SRegion region, long before, long after) {
		list.add(new RecUnit(region, before, after));
	}
	
	public SRegion getFirstRegion() {
		return list.get(list.size() - 1).getRegion();
	}
	
	public void dump(double threshold) {
		//double threshold = 1.5;
		//double threshold = 1.001;
		int limit = 0;
		
		double totalSpeedup = 1;
		/*
		for (int i=0; i<list.size(); i++) {
			totalSpeedup = totalSpeedup * list.get(i).getSpeedup();
			//System.out.println("Acc Speedup = " + totalSpeedup);
			if (totalSpeedup > threshold) {
				limit = i;
				break;
			}
		}*/
		
		//limit = 0;
		limit = list.size();
		double accSpeedup = 1.0;
		int index = 1;
		String header = String.format("%s\t%s\t%s\t%s\t\t%s\t\t%s\t\t%s", 
				"Rank", "SpeedUp", "RegionID", "Src", "Line", "Func", " Type", "SP", "Work"); 
		System.out.println(header);
		for (int i=0; i<limit; i++) {			
			RecUnit unit = list.get(i);
			accSpeedup = accSpeedup * unit.getSpeedup();			
			String output = String.format("[%2d]\t%6.2f \t %s %.2f %.2f", 
					 index++, unit.getSpeedup(), unit.getRegion(), 0.0, 0.0);
			
			System.out.println(output);
		}
	}
}

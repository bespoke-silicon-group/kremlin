package pprof;
import java.util.*;

import planner.Util;

public class CacheStat {
	Map<Integer, List<Stat>> map;
	public CacheStat(String file) {
		List<String> stringList = Util.getStringList(file);
		map = new LinkedHashMap<Integer, List<Stat>>();
		
		for (String each : stringList) {
			String[] splitted = each.split("\t");
			int core = Integer.parseInt(splitted[0].trim());
			double l1ReadMiss = Double.parseDouble(splitted[1].trim());
			double l1WriteMiss = Double.parseDouble(splitted[2].trim());
			double l2ReadMiss = Double.parseDouble(splitted[3].trim());			
			double l2WriteMiss = Double.parseDouble(splitted[4].trim());
			List<Stat> list = new ArrayList<Stat>();
			list.add(new Stat(l1ReadMiss, l1WriteMiss));
			list.add(new Stat(l2ReadMiss, l2WriteMiss));
			map.put(core, list);
		}
	}
	
	double getReadMissRate(int core, int level) {
		for (int each : map.keySet()) {
			if (each >= core) {				
				return map.get(each).get(level).readMiss * 0.01;
			}
		}
		assert(false);
		return 0.0;
	}
	
	double getWriteMissRate(int core, int level) {
		for (int each : map.keySet()) {
			if (each >= core)
				return map.get(each).get(level).writeMiss * 0.01;
		}
		assert(false);
		return 0.0;
	}
	
	void dump() {
		for (int core : map.keySet()) {
			System.out.printf("Core %2d\n", core);
			List<Stat> list = map.get(core);
			for (Stat each : list) {
				System.out.printf(each + "\n");
			}			
		}
	}
	
	class Stat {
		Stat(double readMiss, double writeMiss) {
			this.readMiss = readMiss;
			this.writeMiss = writeMiss;
		}
		double readMiss;
		double writeMiss;
		public String toString() {
			return String.format("r: %.2f, w: %.2f", readMiss, writeMiss);
		}
	}
	
	public static void main(String args[]) {
		CacheStat cache = new CacheStat("F:\\Work\\pact2011\\cg\\cache.txt");
		cache.dump();
	}
}

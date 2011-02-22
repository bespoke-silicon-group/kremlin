package planner;

import java.util.*;
import pprof.*;

public class SRegionInfoSorter {
	public static void sortBySpeedup(List<SRegionInfo> list) {
		class SpeedupComparator implements Comparator {
			public int compare(Object o0, Object o1) {
				SRegionInfo info0 = (SRegionInfo)o0;
				SRegionInfo info1 = (SRegionInfo)o1;
				
				double diff = info0.getSelfSpeedup() - info1.getSelfSpeedup();
				if (diff == 0.0)
					return 0;
				else if (diff > 0)
					return -1;
				else 
					return 1;
			}			
		}
		Collections.sort(list, new SpeedupComparator());		
	}
}

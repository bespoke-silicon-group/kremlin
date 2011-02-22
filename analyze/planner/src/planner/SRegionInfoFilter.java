package planner;
import java.io.*;
import java.util.*;

import pprof.*;

/**
 * SRegionInfoFilter supports several static functinos that 
 * filters out entities that does not meet a specific condition
 * Only entities meeting the specified condition will be included in the return set
 * 
 * @author dhjeon
 *
 */

public class SRegionInfoFilter {
	static public Set<SRegionInfo> filterMinCoverage(Set<SRegionInfo> set, double minCoverage) {
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		for (SRegionInfo each : set) {
			assert(each != null);
			if (each.getCoverage() > minCoverage)
				ret.add(each);
		}
			
		return ret;
	}
	
	static public Set<SRegionInfo> filterMinSpeedup(Set<SRegionInfo> set, double minSpeedup) {
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		for (SRegionInfo each : set) {
			if (each.getSelfSpeedup() > minSpeedup)
				ret.add(each);
		}
		return ret;
	}
	
	static public Set<SRegionInfo> filterMinSpeedupAllLoop(Set<SRegionInfo> set, double minDOALL, double minDOACROSS, double minSP) {
		//System.out.printf("minDOALL = %.2f, minDOACROSS = %.2f, minSP = %.2f\n",
		//		minDOALL, minDOACROSS, minSP);
		
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		for (SRegionInfo each : set) {
			double sp = each.getSelfParallelism();
			double iter = each.getAvgIteration();
			boolean isDOALL = ((sp / iter) > 0.9) ? true : false;
			/*
			if (each.getSRegion().getStartLine() == 798) {
				System.out.println("sp = " + sp + " iter = " + iter + 
						" doall = " + isDOALL + " speedup = " + each.getS);
				assert(false);
			}*/
			if (sp < minSP) {
				System.out.printf("[Filtered by SP] [%.2f < %.2f] %s\n",
						sp, minSP, each.getSRegion());
				continue;
			}
			
			
			if (isDOALL) {
				if (each.getSelfSpeedup() > minDOALL)
					ret.add(each);
			} else {
				if (each.getSelfSpeedup() > minDOACROSS)
					ret.add(each);				
			}
		}
		return ret;
	}
	
	static public List<SRegionInfo> filterMinSpeedup(List<SRegionInfo> list, double minSpeedup) {
		List<SRegionInfo> ret = new ArrayList<SRegionInfo>();
		for (SRegionInfo each : list) {
			if (each.getSelfSpeedup() > minSpeedup)
				ret.add(each);
		}
		return ret;
	}
	
	static public Set<SRegionInfo> filterRegionType(Set<SRegionInfo> set, RegionType allowedType) {
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		for (SRegionInfo each : set) {
			if (each.getSRegion().getType() == allowedType)
				ret.add(each);
		}			
		return ret;
	}
	
	
	static public Set<SRegionInfo> filterNonDoall(SRegionInfoAnalyzer analyzer, Set<SRegionInfo> set, double minSP) {
		Set<SRegionInfo> loopSet = filterRegionType(set, RegionType.LOOP);
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		
		for (SRegionInfo info : loopSet) {
			//Set<SRegion> children = info.getChildren();
			//assert(children.size() == 1);
			//SRegion child = children.iterator().next();
			//SRegionInfo childInfo = analyzer.getSRegionInfo(child);
			
			double iter = info.getAvgIteration();						
			double sp = info.getSelfParallelism();			
			//if (sp >= (iter * 0.9) && iter > 2.0)
			//if (sp >= (iter * 0.9))
			if (sp >= minSP)
				ret.add(info);
		}			
		return ret;
	}
	
static public List<SRegionInfo> filterNonOuterDoall(SRegionInfoAnalyzer analyzer, List<SRegionInfo> list, double minSP) {
		List<SRegionInfo> temp = filterNonDoall(analyzer, list, minSP);
		Set<SRegionInfo> removeSet = new HashSet<SRegionInfo>();
		for (SRegionInfo each : temp) {			
			SRegionInfoGroup group = analyzer.getRegionGroup(each.getSRegion());
			boolean parallelLoopDetected = false;			
			for (int i=0; i<group.size(); i++) {
				SRegionInfo info = group.getEntry(i);				
				SRegion region = info.getSRegion();				
				if (region.getType() != RegionType.LOOP)
					continue;
				
				if (temp.contains(info)) {
					if (parallelLoopDetected) {						
						//removeSet.add(info);						
						
					} else {
						parallelLoopDetected = true;						
						//ret.remove(region);
					}
				}
			}
		}	
		temp.removeAll(removeSet);
		return temp;	
	}

	public static List<SRegionInfo> filterExcludedRegions(SRegionInfoAnalyzer analyzer, List<SRegionInfo> list, String file) {
		List<Long> excludeList = new ArrayList<Long>();
		List<SRegionInfo> ret = new ArrayList<SRegionInfo>();
		if (file == null) {
			return new ArrayList<SRegionInfo>(list);
		}
		
		try {
			BufferedReader reader = new BufferedReader(new FileReader(file));
			while (true) {
				String line = reader.readLine();
				if (line == null)
					break;
				if (line.length() < 1)
					break;
				excludeList.add(Long.parseLong(line));
				
				//System.out.println("@" + line);
			}
			
		} catch(Exception e) {
			e.printStackTrace();		
		}
		
		for (SRegionInfo info : list) {
			SRegion region = info.getSRegion();
			if (excludeList.contains(region.getID())) {
				System.out.println("exclude line: " + region.getStartLine());
			} else
				ret.add(info);
		}
		//assert(false);
		return ret;
	}
	
	static public List<SRegionInfo> filterNonDoall(SRegionInfoAnalyzer analyzer, List<SRegionInfo> list, double minSP) {		
		List<SRegionInfo> ret = new ArrayList<SRegionInfo>();
		
		for (SRegionInfo info : list) {
			if (info.getSRegion().getType() != RegionType.LOOP)
				continue;
			
			Set<SRegion> children = info.getChildren();
			assert(children.size() == 1);
			SRegion child = children.iterator().next();
			SRegionInfo childInfo = analyzer.getSRegionInfo(child);
			
			double iter = info.getAvgIteration();						
			double sp = info.getSelfParallelism();			
									
			if (sp >= minSP)
				ret.add(info);						
		}			
		return ret;
	}
	
	static public Set<SRegion> toSRegionSet(Set<SRegionInfo> set) {
		Set<SRegion> ret = new HashSet<SRegion>();
		for (SRegionInfo info : set)
			ret.add(info.getSRegion());
		
		return ret;
	}
	
	static public List<SRegion> toSRegionList(List<SRegionInfo> list) {
		List<SRegion> ret = new ArrayList<SRegion>();
		for (SRegionInfo info : list)
			ret.add(info.getSRegion());
		
		return ret;
	}
	
	static public Set<SRegionInfo> toSRegionInfoSet(SRegionInfoAnalyzer analyzer, Set<SRegion> set) {
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		for (SRegion region : set) {
			SRegionInfo info = analyzer.getSRegionInfo(region);
			if (info != null)
				ret.add(info);
		}
		
		return ret;
	}
	
	public static List<SRegionInfo> toSRegionInfoList(SRegionInfoAnalyzer analyzer, List<SRegion> list) {
		List<SRegionInfo> ret = new ArrayList<SRegionInfo>();
		for (SRegion region : list) {
			SRegionInfo info = analyzer.getSRegionInfo(region);
			if (info != null)
				ret.add(info);
		}
		
		return ret;
	}
	
	public static List<SRegion> toCompactList(List<SRegion> list) {
		List<SRegion> ret = new ArrayList<SRegion>();
		Set<Integer> retired = new HashSet<Integer>();
		
		for (SRegion each : list) {
			int start = each.getStartLine();
			if (!retired.contains(start)) {
				retired.add(start);
				ret.add(each);
			}				
		}
		return ret;		
	}
}

package pyrplan;
import pprof.*;
import java.util.*;

public class FilterControl {
	double minDOALL;
	double minDOACROSS;
	double outerIncentive;
	double minSP;
	String filterFile;
	Set<RegionType> forbiddenTypes;
	
	public FilterControl(double minSP, double minDOALL, double minDOACROSS, double outerIncentive) {
		this.minSP = minSP;
		this.minDOACROSS = minDOACROSS;
		this.minDOALL = minDOALL;
		this.outerIncentive = outerIncentive;
		this.forbiddenTypes = new HashSet<RegionType>();
	}
	
	boolean isDoall(SRegionInfo info) {
		double iter = info.getAvgIteration();
		double sp = info.getSelfParallelism();
		if (info.getSRegion().getType() != RegionType.LOOP)
			return false;
		
		return (sp > iter * 0.9);
	}
	
	boolean isDoacross(SRegionInfo info) {
		double iter = info.getAvgIteration();
		double sp = info.getSelfParallelism();
		if (info.getSRegion().getType() != RegionType.LOOP)
			return false;
		
		return (sp < iter * 0.9);
	}
	
	public Set<SRegionInfo> getPostFilterSRegionInfoSet(SRegionInfoAnalyzer analyzer) {
		Set<SRegionInfo> ret = new HashSet<SRegionInfo>();
		
		Set<SRegionInfo> set = analyzer.getSRegionInfoSet();
		
		for (RegionType each : forbiddenTypes) {
			System.out.println("Filter Type: " + each);			
		}
		//System.out.println("minSP = " + minSP + " minDOALL = " + minDOALL + " minDOACROSS = " + minDOACROSS);
		
		for (SRegionInfo each : set) {
			RegionType type = each.getSRegion().getType();
			double sp = each.getSelfParallelism();
			double speedup = each.getSelfSpeedup();
			System.out.println(each);
			int startLine = each.getSRegion().getStartLine();			
			System.out.println("sp = " + sp +  " speedup = " + speedup);			
			//System.out.println("work = " + each.getAvgWork() + " sp = " + each.getSelfParallelism() + 
			//	" speedup = " + speedup + " coverage = " + each.getCoverage());
			
			if (forbiddenTypes.contains(type))
				continue;
			if (each.getAvgWork() < 1) {
				continue;
			}
			if (sp < this.minSP)
				continue;
			
			if (isDoall(each) && speedup > minDOALL)  {				
				ret.add(each);	
				
			} else if (speedup > minDOACROSS){
				ret.add(each);				
			}
			/*
			else if (speedup < minDOACROSS) {
				System.out.println("[Small DOACROSS]" + each.getSRegion());
				continue;
			}*/
			
			
		}		
		return ret;
	}
	
	public void setFilterFile(String file) {
		this.filterFile = file;
	}
	
	public void filterByRegionType(RegionType type) {
		forbiddenTypes.add(type);
	}
	
	public String getFileName() {
		return this.filterFile;
	}
	
	public double getMinSP() {
		return minSP;		
	}
	
	public double getMinDOALL() {
		return minDOALL;
	}
	
	public double getMinDOACROSS() {
		return minDOACROSS;
	}
	
	public double getOuterIncentive() {
		return outerIncentive;
	}
	
	public String toString() {
		String ret = "minSP = " + minSP + " minDOALL = " + minDOALL + " minDOACROSS = " + minDOACROSS;
		return ret;
	}
}

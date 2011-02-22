package pprof;

import java.util.*;

import planner.ParameterSet;
public class SRegionInfo implements Comparable {
	SRegion region;
	
	long numInstance;
	long totalWork;	
	long avgWork;
	long adjustedAvgWork;
	long totalCP;
	double totalSPWork;
	double totalTPWork;
	double workCoverage;
	double avgCP;
	double sumTotalParallelism;
	//double sumSelfParallelism;
	double selfParallelism;
	double totalParallelism;	
	double selfSpeedup;	
	double maxSP;
	double avgIter;
	double sdWorkPercent;	// standard deviation of Work
	long totalReadCnt;
	long totalWriteCnt;
	long totalReadLineCnt;
	long totalWriteLineCnt;
	double avgReadCnt;
	double avgWriteCnt;
	double avgReadLineCnt;
	double avgWriteLineCnt;
	long totalIter;
	
	
	RegionStatus regionStatus;
	RegionStatus exploitStatus;
	
	//double totalSpeedup;	
	Set<URegion> set;
	Set<SRegion> parents;
	Set<SRegion> children;
	FreqAnalyzer freq;
	//SelfParallelismAnalyzer rp;
	
	SRegionInfo(SRegion region, Set<URegion> set, FreqAnalyzer freq, SelfParallelismAnalyzer rp, long appTotalWork) {
		this.region = region;
		this.set = new HashSet<URegion>(set);
		parents = new HashSet<SRegion>();
		children = new HashSet<SRegion>();
		this.freq = freq;		
		this.regionStatus = RegionStatus.SERIAL;
		this.exploitStatus = RegionStatus.SERIAL;
		
		build(appTotalWork);
	}
	
	public boolean isLeaf() {
		return (children.size() == 0);
	}
	
	public SRegion getSRegion() {
		return this.region;
	}
	
	public long getInstanceCount() {
		return this.numInstance;
	}
	
	public double getCoverage() {
		return this.workCoverage;		
	}
	
	public double getSelfParallelism() {
		return this.selfParallelism;
	}
	
	
	
	public double getNSP() {
		return 1.0 - (1.0 / this.selfParallelism);
	}
	
	public long getTotalWork() {
		return this.totalWork;
	}
	
	public double getAvgCP() {
		return this.avgCP;
	}
	
	public double getAvgWork() {
		return this.avgWork;
	}
	
	public double getAvgBwWork() {
		return this.avgReadLineCnt * 200 + getAvgWork();
	}
	
	public double getParallelBwWork(int n) {
		int bwMultiplier = n;
		if (n > 8)
			bwMultiplier = 8;
		
		int cacheSizeChip = 1024 * 1024 * 6;
		int nLinesChip = cacheSizeChip / (64 * 16); 
		int bestCacheFetch = (int)this.avgReadLineCnt - nLinesChip * bwMultiplier;
		if (bestCacheFetch < 0)
			bestCacheFetch = 0;
		return bestCacheFetch * 400 / bwMultiplier + getAvgWork() / n;
	}
	
	public double getParallelBwWorkMax(int n) {
		int bwMultiplier = n;
		if (n > 8)
			bwMultiplier = 8;
		
		int cacheSizeChip = 1024 * 1024 * 6;
		int nLinesChip = cacheSizeChip / (64 * 16); 
		int bestCacheFetch = (int)this.avgReadLineCnt;
		if (bestCacheFetch < 0)
			bestCacheFetch = 0;
		return bestCacheFetch * 400 / bwMultiplier + getAvgWork() / n;
	}
	
	public double getTimeReduction() {
		return this.getCoverage() - this.getCoverage() / this.getSelfParallelism();
		
	}
	
	public double getAvgIterWork() {
		if (region.getType() == RegionType.LOOP)
			return this.avgWork / this.avgIter;
		else
			return -1.0;
	}
	
	public double getTotalParallelism() {
		return this.totalParallelism;
	}
	
	public double getSelfSpeedup() {
		return this.selfSpeedup;
	}	
	
	
	public double getWorkDeviationPercent() {		
		return this.sdWorkPercent;
	}
	
	public Set<SRegion> getParents() {
		return this.parents;
	}
	
	public Set<SRegion> getChildren() {
		return this.children;
	}	
	
	public void setParallelStatus(RegionStatus status) {
		assert(status == RegionStatus.PARALLEL || status == RegionStatus.SERIAL);
		this.exploitStatus = status;
	}
	
	public void setRegionStatus(RegionStatus status) {
		this.regionStatus = status;
	}
	
	public RegionStatus getRegionStatus() {
		return this.regionStatus;
	}
	
	public boolean isExploited() {
		return (this.exploitStatus == RegionStatus.PARALLEL);
	}
	
	public double getMemReadRatio() {
		final int loadWeight = 4;
		return this.avgReadCnt * loadWeight / this.avgWork;
	}
	
	public double getAvgMemWriteCnt() {		
		return this.avgWriteCnt;
	}
	
	public double getAvgMemReadCnt() {		
		return this.avgReadCnt;
	}
	
	public double getMemReadLineRatio() {
		final int loadWeight = 4;
		return this.avgReadLineCnt * loadWeight / this.avgWork;
	}
	
	public double getMemWriteRatio() {		
		final int storeWeight = 1;
		return this.avgWriteCnt * storeWeight / this.avgWork;
	}
	
	public double getMemWriteLineRatio() {		
		final int storeWeight = 1;
		return this.avgWriteLineCnt * storeWeight / this.avgWork;
	}
	
	public double getBandwidthMaxCore(double saturateRatio) {		 
		return saturateRatio / (this.getMemReadLineRatio() + this.getMemWriteLineRatio());		
		
		//return maxSpeedup;
		
	}
	
	// calculate the standard deviation of loop iterations
	
	double calcSdWork() {
		double totalSum = 0.0;
		for (URegion entry : set) {
			double sum = 0.0;
			Map<URegion, Long> map = entry.children;
			double avg = entry.work / (double)entry.getChildrenCount();
			for (URegion each : map.keySet()) {
				double diff = (double)each.work - avg;
				sum += diff * diff;
			}
			double sd = Math.sqrt(sum / entry.getChildrenCount());
			double percent = sd / avg * 100.0;
			totalSum += percent * entry.cnt;
			//double diff = (double)entry.work - avg;
			//sum += diff * diff * entry.cnt;
		}
		
		return totalSum / this.numInstance; 			
	}
	
	void canonicalSet() {
		// 1) sort by work
		// 2) find entry set
		// 3) return set of entry set
		for (URegion entry : set) {
			
		}
	}
	
	void build(long appTotalWork) {		
		this.workCoverage = 0;
		this.numInstance = 0;
		this.totalWork = 0;
		this.sumTotalParallelism = 0;
		this.totalSPWork = 0;
		this.totalTPWork = 0;
		this.totalCP = 0;
		this.maxSP = 0.0;
		this.totalIter = 0;
		this.totalReadCnt = 0;
		this.totalWriteCnt = 0;
		this.totalReadLineCnt = 0;
		this.totalWriteLineCnt = 0;
		
		
		
		for (URegion entry : set) {
			Set<URegion> parentSet = entry.parentSet;			
			Set<URegion> childrenSet = entry.getChildrenSet();
			for (URegion each : parentSet) {
				parents.add(each.sregion);
			}

			for (URegion each : childrenSet) {
				children.add(each.sregion);
			}
			//assert(freq.getFreq(entry) == entry.cnt);
			this.numInstance += entry.cnt;
			//this.workCoverage += freq.getCoveragePercentage(entry);
			//this.workCoverage += entry.cnt * 
			this.totalWork += entry.cnt * entry.work;
			this.totalCP += entry.cnt * entry.cp;
			if (entry.cp != 0)
				this.sumTotalParallelism += entry.cnt * (entry.work / (double)entry.cp);

			if (entry.getSelfParallelism() < 1.0) {
				System.out.println(entry);
				assert(false);
			}

			if (entry.getSelfParallelism() > maxSP)
				maxSP = entry.getSelfParallelism();

			if (region.getType() == RegionType.LOOP) {
				for (URegion child : entry.getChildrenSet()) {
					totalIter += entry.cnt * entry.getChildCount(child);
				}
			}
			if (entry.getSelfParallelism() < 1.0 && entry.getSelfParallelism() > 0.0) {
				System.err.println(entry);
				assert(false);
			}
			
			assert(entry.getSelfParallelism() >= 0.99);
			this.totalSPWork += entry.cnt * (entry.work / entry.getSelfParallelism());
			this.totalTPWork += entry.cnt * (entry.work / entry.getParallelism());
			//this.sumSelfParallelism += entry.cnt * entry.getSelfParallelism();
			this.totalReadCnt += entry.cnt * entry.readCnt;
			this.totalWriteCnt += entry.cnt * entry.writeCnt;
			this.totalReadLineCnt += entry.cnt * entry.readLineCnt;
			this.totalWriteLineCnt += entry.cnt * entry.writeLineCnt;
			if (region.getStartLine() == 153 && region.getType() == RegionType.LOOP) {
				System.out.println(entry);
			}
			//System.out.println(entry);			
		}
		
		this.avgWork = totalWork / numInstance;		
		this.selfParallelism = (double)totalWork / (double)totalSPWork;
		this.totalParallelism = (double)totalWork / (double)totalTPWork;
		this.avgCP = (double)totalCP / (double)numInstance;		
		this.avgIter = (double)totalIter / numInstance;		
		this.sdWorkPercent = calcSdWork();
		
		this.avgReadCnt = (double)this.totalReadCnt / numInstance;
		this.avgWriteCnt = (double)this.totalWriteCnt / numInstance;
		this.avgReadLineCnt = (double)this.totalReadLineCnt / numInstance;
		this.avgWriteLineCnt = (double)this.totalWriteLineCnt / numInstance;
		this.workCoverage = ((double)this.totalWork / (double)appTotalWork) * 100.0;
		this.selfSpeedup = 100.00 / (100.00 - (this.workCoverage - this.workCoverage / (double)this.selfParallelism));
		
		//assert(false);
		if (selfParallelism < 1.0) {			
			System.out.println(this);			
			assert(false);
		}

		
		double cacheMissRatio = getMemReadLineRatio() + getMemWriteLineRatio();	// out of total work		
		this.adjustedAvgWork = (long)(cacheMissRatio * 33.0 + (1.0 - cacheMissRatio)) * this.avgWork;
	}
	
	public double getAvgIteration() {
		return this.avgIter;
	}
	
	
		
	/*
	public double getSelfReductionPercent() {
		return this.workCoverage - this.workCoverage / this.selfParallelism;
	}	
	
	public double getReductionPercentByMPC() {
		if (this.selfParallelism >= this.getMaxProfitableCore())
			return this.workCoverage - this.workCoverage / this.getMaxProfitableCore();
		else
			return this.getSelfReductionPercent();
	}
	
	public double getReductionPercentByCore(int core) {
		if ((double)core < this.getMaxProfitableCore() &&
				(double)core < this.getSelfParallelism())
			return this.workCoverage - this.workCoverage / core;
		else
			return getReductionPercentByMPC();
	}*/
	
	public Set<URegion> getInstanceSet() {
		return this.set;
	}
	
	
	public String toString() {
		StringBuffer buffer = new StringBuffer();
		
		for (URegion region : set) {
			buffer.append(region + "\n");
		}
		
		return String.format("%s: \n" +
				"\t# of dynamic instances: %d \n" +
				"\tSelf Parallelism: %.2f\n" +
				"\tTotal Parallelism: %.2f\n" +
				"\tCoverage: %.2f\n" + 
				"\tSpeedup: %.2fx\n\n" +
				"URegion:\n" + 
				"%s\n",		  
				region, this.numInstance, this.selfParallelism, 
				this.totalParallelism, this.workCoverage, this.selfSpeedup, 
				buffer.toString());
		
	}
	

	@Override
	public int compareTo(Object arg) {
		SRegionInfo target = (SRegionInfo)arg;
		double diff = this.getSelfSpeedup() - target.getSelfSpeedup();
		return (diff > 0.0) ? -1 : 1; 
	}
}
 
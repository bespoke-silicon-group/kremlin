package pprof;

import java.util.*;

public class CRegion implements Comparable {
	static long allocatedId = 0;
	CRegion(SRegion sregion, Set<URegion> set, List<Long> context, long totalWork) {
		this.id = allocatedId++;
		this.region = sregion;
		this.set = set;
		this.context = context;
		Set<URegion> parentSet = set.iterator().next().getParentSet();
		this.parentSRegion = (parentSet.size() > 0) ? parentSet.iterator().next().getSRegion() : null;
		this.children = new HashSet<CRegion>();
		build(totalWork);
	}
	
	SRegion getParentSRegion() {
		return this.parentSRegion;
	}
	
	List<Long> getContext() {
		return this.context;
	}
	
	void setParent(CRegion parent) {
		this.parent = parent;
		parent.addChild(this);	

	}
	
	void addChild(CRegion child) {
		this.children.add(child);
	}
	
	public long getExclusiveWork() {
		long ret = this.totalWork;
		for (CRegion each : this.children) {
			ret -= each.totalWork;
		}
		return ret;
	}
	long id;
	SRegion region;
	SRegion parentSRegion;
	List<Long> context;
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
	public double totalParallelism;	
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
	
	Set<URegion> set;
	CRegion parent;
	Set<CRegion> children;
	
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
				//parents.add(each.sregion);
			}

			for (URegion each : childrenSet) {
				//children.add(each.sregion);
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
		//this.sdWorkPercent = calcSdWork();
		
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

		
		//double cacheMissRatio = getMemReadLineRatio() + getMemWriteLineRatio();	// out of total work		
		//this.adjustedAvgWork = (long)(cacheMissRatio * 33.0 + (1.0 - cacheMissRatio)) * this.avgWork;
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
	
	public Set<CRegion> getChildrenSet() {
		return this.children;
	}
	
	public CRegion getParent() {
		return this.parent;
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
	
	@Override
	public int compareTo(Object arg) {
		CRegion target = (CRegion)arg;
		//double diff = this.getSelfSpeedup() - target.getSelfSpeedup();
		double diff = 0.1;
		return (diff > 0.0) ? -1 : 1; 
	}
	
	public String toString() {
		String ret = String.format("[%d] %s work = %d, sp = %.2f, coverage = %.2f children = %d", 
				this.id, this.region, this.avgWork, this.selfParallelism, this.workCoverage, this.children.size());
		return ret;
	}
}

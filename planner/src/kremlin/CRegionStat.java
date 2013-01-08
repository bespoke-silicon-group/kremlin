package kremlin;

/**
 * TraceStat: corresponds to CStat in CRegion.c file
 *
 */
public class CRegionStat {
	long work;
	long tpWork;
	long spWork;
	long nInstance;	
	long totalIter, minIter, maxIter;	
	
	double minSP, maxSP;
	double rWeight;	// recursion weight
	
	CRegionStat(long nInstance, long work, long tpWork, long spWork, double minSP, double maxSP, long totalIter, long minIter, long maxIter) {
		this.nInstance = nInstance;
		this.work = work;
		this.tpWork = tpWork;
		this.spWork = spWork;
		this.minSP = minSP;
		this.maxSP = maxSP;
		this.totalIter = totalIter;
		this.minIter = minIter;
		this.maxIter = maxIter;
		this.rWeight = 0.0;
	}
	
	public long getTotalWork() {
		return this.work;
	}
	
	public long getAvgWork() {
		return getTotalWork() / this.nInstance;
	}
	
	public double getSelfP() {
		return this.work / (double)this.spWork;
	}
	
	public double getMinSelfP() {
		return this.minSP;		
	}
	
	public double getMaxSelfP() {
		return this.maxSP;		
	}
	
	public long getMinIter() {
		return minIter;
	}
	
	public long getMaxIter() {
		return maxIter;
	}
	
	public double getAvgIter() {
		return totalIter / this.nInstance;
	}
	
	public void setRecursionWeight(double weight) {
		this.rWeight = weight;
	}
	
	public double getRecursionWeight() {
		return this.rWeight;
	}
	
	public String toString() {
		String ret = String.format("selfP = %.2f, iter = %.2f, avg work = %d, count = %d",  
				this.getSelfP(), this.getAvgIter(), this.getAvgWork(), this.nInstance);
		return ret;
	}
}

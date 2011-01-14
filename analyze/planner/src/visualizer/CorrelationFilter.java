package visualizer;

public class CorrelationFilter {
	double minSelfP;
	double minTotalP;
	double maxSelfP;
	double maxTotalP;
	double minCoverage;
	double maxCoverage;	
	
	int entryCnt;
	int parallelCnt;
	int ancestorParallelCnt;
	public enum RegionType {REGION_ALL, REGION_FUNC, REGION_LOOP};	
	RegionType type;
	
	CorrelationFilter(RegionType type) {
		this.type = type;		
		this.minSelfP = -1.0;
		this.maxSelfP = -1.0;
		this.minTotalP = -1.0;
		this.maxTotalP = -1.0;
		this.minCoverage = -1.0;
		this.maxCoverage = -1.0;	
		
		this.entryCnt = 0;
		this.parallelCnt = 0;
		this.ancestorParallelCnt = 0;
	}
	
	void setMinCoverage(double min) {
		this.minCoverage = min;
	}
	
	void setMaxCoverage(double max) {
		this.maxCoverage = max;
	}
	
	void setMinSelfP(double min) {
		this.minSelfP = min;
	}
	
	void setMaxSelfP(double max) {
		this.maxSelfP = max;
	}
	
	void setMinTotalP(double min) {
		this.minTotalP = min;
	}
	
	void setMaxTotalP(double max) {
		this.maxTotalP = max;
	}
	
	void process(CorrelationEntry entry) {
		if (type == RegionType.REGION_LOOP && entry.type == 1)
			return;
		
		if (type == RegionType.REGION_FUNC && entry.type == 0)
			return;
		
		if (minSelfP >= 0 && entry.selfP < minSelfP)
			return;
		
		if (maxSelfP >= 0 && entry.selfP > maxSelfP)
			return;
		
		if (minTotalP >= 0 && entry.totalP < minTotalP)
			return;
		
		if (maxTotalP >= 0 && entry.totalP > maxTotalP)
			return;
		
		if (minCoverage >= 0 && entry.coverage < minCoverage)
			return;
		
		if (maxCoverage >= 0 && entry.coverage > maxCoverage)
			return;
		
		this.entryCnt++;
		if (entry.isParallelized == 1)
			this.parallelCnt++;
		else if (entry.isAncestorParallelized == 1)
			this.ancestorParallelCnt++;
	}
	/*
	public String printInfo(InfoType type) {
		if (type == InfoType.SELF) {
			return selfParallelismString();
			
		} else if (type == InfoType.TOTAL) {
			return totalParallelismString();
			
		} else if (type == InfoType.ANCESTOR) {
			return ancestorParallelStatString();
		}
		assert(false);
		return null;
	}*/
	
	public String filterString() {
		StringBuffer buffer = new StringBuffer();
		buffer.append("Type: " + this.type + "\n");
		if (minCoverage > 0)
			buffer.append(String.format("minCoverage: %6.2f\n", this.minCoverage));
		if (maxCoverage > 0)
			buffer.append(String.format("maxCoverage: %6.2f\n", this.maxCoverage));
		
		if (minSelfP > 0)
			buffer.append(String.format("minSelfP: %6.2f\n", this.minSelfP));
		if (maxSelfP > 0)
			buffer.append(String.format("maxSelfP: %6.2f\n", this.maxSelfP));	
			
		if (minTotalP > 0)
			buffer.append(String.format("minTotalP: %6.2f\n", this.minTotalP));
		if (maxTotalP > 0)
			buffer.append(String.format("maxTotalP: %6.2f\n", this.maxTotalP));	
			
		return buffer.toString();
	}
	
	public String parallelStatString() {		
		String ret = String.format("%3d/%3d (%5.2f%%)", 
				this.parallelCnt, this.entryCnt,
				(double)this.parallelCnt * 100.0 / this.entryCnt				
				);
		return ret;
	}
	
	
	
	public String ancestorParallelStatString() {
		int total = this.parallelCnt + this.ancestorParallelCnt;
		String ret = String.format("%3d/%3d (%5.2f%%)", 
				total, this.entryCnt,				
				(double)total * 100.0 / this.entryCnt
				);
		return ret;
	}
	
	
	
	public String toString() {		
		String condition = String.format(
				"type: %s selfP[%.2f, %.2f], totalP[%.2f, %.2f], coverage[%.2f, %.2f]",
				this.type, this.minSelfP, this.maxSelfP, 
				this.minTotalP, this.maxTotalP, this.minCoverage, this.maxCoverage);
		String counts = String.format("total: %d, parallelized: %d, ancestor parallelized %d",
				this.entryCnt, this.parallelCnt, this.ancestorParallelCnt);
		return String.format("%s\n%s", condition, counts);
	}
	
	
}

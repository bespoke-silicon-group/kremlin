package pprof;

import java.util.*;

enum CRegionType {
	NORMAL, 
	REC_INIT, 
	REC_SINK,
	REC_NORM;
	
	public String toString() {
		if (this == NORMAL)
			return "Norm";
		else if (this == REC_INIT)
			return "RInit";
		else if (this == REC_SINK)
			return "RSink";
		else if (this == REC_NORM)
			return "RNorm";
		else
			return "ERR";
	}
};


public abstract class CRegion implements Comparable {
	CRegionType type;	
	long id;
	SRegion region;
	SRegion parentSRegion;
	CallSite callSite;	
	long numInstance;
	long totalWork;	
	boolean pbit;
	
	
	CRegion parent;
	Set<CRegion> children;
	
	public static CRegion create(SRegion sregion, CallSite callSite, TraceEntry entry) {
		if (entry.type > 0)
			return new CRegionR(sregion, callSite, entry);
		else
			return new CRegionN(sregion, callSite, entry);
	}

	CRegion(SRegion sregion, CallSite callSite, TraceEntry entry) {
		this.region = sregion;
		this.callSite = callSite;
		this.id = entry.uid;
		this.totalWork = entry.work;
		this.numInstance = entry.cnt;
		this.children = new HashSet<CRegion>();
		
		if (entry.type == 0)
			this.type = CRegionType.NORMAL;
		else if (entry.type == 1)
			this.type = CRegionType.REC_INIT;
		else if (entry.type == 2)
			this.type = CRegionType.REC_SINK;
		else if (entry.type == 3)
			this.type = CRegionType.REC_NORM;
		else {
			assert(false);
		}
	}
	
	CRegionType getRegionType() {
		return this.type;
	}
	
	SRegion getParentSRegion() {
		return this.parentSRegion;
	}	
	
	void setParent(CRegion parent) {
		this.parent = parent;
		parent.addChild(this);
	}
	
	void addChild(CRegion child) {
		this.children.add(child);		
	}
		
	public long getExclusiveWork() {
		long ret = this.getTotalWork();
		for (CRegion each : this.children) {
			ret -= each.getTotalWork();
		}
		return ret;
	}	
	
	public SRegion getSRegion() {
		return this.region;
	}
	
	public long getInstanceCount() {
		return this.numInstance;
	}
		
	public Set<CRegion> getChildrenSet() {
		return this.children;
	}
	
	public CRegion getParent() {
		return this.parent;
	}
	
	public boolean getParallelBit() {
		return this.pbit;
	}
	
	abstract public double getSelfP();	
	abstract public long   getTotalWork(); 
	abstract public long   getAvgWork();
		
	
	@Override
	public int compareTo(Object arg) {
		CRegion target = (CRegion)arg;
		//double diff = this.getSelfSpeedup() - target.getSelfSpeedup();
		double diff = 0.1;
		return (diff > 0.0) ? -1 : 1; 
	}
	
	public PType getParallelismType() {
		RegionType type = getSRegion().getType(); 
		if (getChildrenSet().size() == 0)
			return PType.ILP;

		if (type == RegionType.LOOP) {
			if (this.getParallelBit())
				return PType.DOALL;
			else
				return PType.DOACROSS;
		}
		return PType.TLP;
	}
	
	public String toString() {
		String ret = String.format("[%d] %s work = %d, sp = %.2f, children = %d", 
				this.id, this.region, this.getAvgWork(), this.getSelfP(), this.children.size());
		if (callSite != null && region.type == RegionType.FUNC) {
			ret = ret + "\t" + callSite;				
		}		
		return ret;
	}
	
	public abstract String getStatString();
	
	
	// graveyard
	
	//public double getCoverage() {
	//	return this.workCoverage;		
	//}
	
	//public double getAvgCP() {
	//	return this.avgCP;
	//}
	

}

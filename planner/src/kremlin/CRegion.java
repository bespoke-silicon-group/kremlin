package kremlin;

import java.util.*;

/*
 * Class that represents a "CRegion" (i.e. compressed region) from kremlin's
 * profiling stage.
 */
public abstract class CRegion implements Comparable {
	CRecursiveType type;	
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
		this.pbit = entry.pbit;
		this.children = new HashSet<CRegion>();
		
		if (entry.type == 0)
			this.type = CRecursiveType.NORMAL;
		else if (entry.type == 1)
			this.type = CRecursiveType.REC_INIT;
		else if (entry.type == 2)
			this.type = CRecursiveType.REC_SINK;
		else if (entry.type == 3)
			this.type = CRecursiveType.REC_NORM;
		else {
			assert(false);
		}
	}

	/* Getter methods. */
	public CallSite getCallsite() { return this.callSite; }
	public CRecursiveType getRecursiveType() { return this.type; }
	public long getId() { return this.id; }
	public CRecursiveType getRegionType() { return this.type; }
	public SRegion getParentSRegion() { return this.parentSRegion; }	
	public SRegion getSRegion() { return this.region; }
	public long getInstanceCount() { return this.numInstance; }
	public Set<CRegion> getChildrenSet() { return this.children; }
	public CRegion getParent() { return this.parent; }
	public boolean getParallelBit() { return this.pbit; }
	public boolean isLeaf() { return children.size() == 0; }

	/*
	 * "Exclusive work" is amount of work done in this CRegion but not in a
	 * child CRegion.
	 */
	public long getExclusiveWork() {
		long ret = this.getTotalWork();
		for (CRegion each : this.children) {
			ret -= each.getTotalWork();
		}
		return ret;
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
	
	
	/*
	 * These are abstract getters because the way we calculate these values
	 * will depend on what type of region it is.
	 */
	abstract public double getSelfP();	
	abstract public double getMinSelfP();	
	abstract public double getMaxSelfP();
	abstract public double getTotalParallelism();	
	abstract public long   getTotalWork(); 
	abstract public long   getAvgWork();
	abstract public CRegionStat getRegionStat();
	public abstract String getStatString();

	/* Setter methods. */
	void setParent(CRegion parent) {
		this.parent = parent;
		parent.addChild(this);
	}
	
	void addChild(CRegion child) {
		this.children.add(child);		
	}
	
	
	@Override
	public int compareTo(Object arg) {
		CRegion target = (CRegion)arg;
		//double diff = this.getSelfSpeedup() - target.getSelfSpeedup();
		double diff = 0.1;
		return (diff > 0.0) ? -1 : 1; 
	}
	
	public String toString() {
		String ret = String.format("[%d] %s work = %d, sp = %.2f, children = %d", 
				this.id, this.region, this.getAvgWork(), this.getSelfP(), this.children.size());
		if (callSite != null && region.type == RegionType.FUNC) {
			ret = ret + "\t" + callSite;				
		}		
		return ret;
	}
}

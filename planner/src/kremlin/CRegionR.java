package kremlin;
import java.util.ArrayList;
import java.util.List;

import kremlin.*;

/*
 * Class to represent a recursive CRegion.
 * It is meant to be immutable so it only contains getters.
 */
public class CRegionR extends CRegion {	
	List<CRegionStat> stats;
	double selfP;
	double totalP;
	CRegion recursionTarget;
	
	CRegionR(SRegion sregion, CallSite callSite, TraceEntry entry) {
		super(sregion, callSite, entry);
		buildFromTraceEntry(entry);
		
	}
	
	void buildFromTraceEntry(TraceEntry entry) {			
		this.stats = new ArrayList<CRegionStat>();
		this.stats.addAll(entry.statList);
		this.pbit = entry.pbit;
		this.numInstance = entry.cnt;	
		this.totalWork = entry.work;		
		this.selfP = 0.0;
		this.totalP = 0.0;
	}
	
	public CRegionStat getStat(int index) {
		assert(this.stats.size() > index);
		return this.stats.get(index);
	}
	
	public int getStatSize() { return this.stats.size(); }
	
	double computeSelfP() {	
		double saving = 0.0;
		//System.err.printf("[computeSelfP] Node = %d\n", this.id);
		for (int i=this.getStatSize()-1; i>=0; i--) {
			CRegionStat stat = this.getStat(i);
			double adjusted = stat.getTotalWork() - saving * stat.rWeight;
			double nextSaving = stat.getTotalWork() - adjusted / stat.getSelfP();
			//System.err.printf("\t %d: total = %d, selfP = %.2f prev saving = %.2f, adjusted = %.2f, next saving = %.2f\n", 
			//		i, stat.getTotalWork(), stat.getSelfP(), saving, adjusted, nextSaving);
			saving = nextSaving;
			
		}
		
		double totalWork = this.getStat(0).getTotalWork();
		double selfP = totalWork / (totalWork - saving);		
		return selfP;
	}
	
	public void setRecursionTarget(CRegion target) {
		assert(target != null);		
		this.recursionTarget = target;
	}
	
	public CRegion getRecursionTarget() { return this.recursionTarget; }
	
	// XXX: FIXME!
	public double getTotalParallelism() { return -1.0; }

	public double getSelfP() {
		if (this.selfP < 1.0)
			this.selfP = this.computeSelfP();
		
		return this.selfP;
	}
	
	public double getMinSelfP() {
		double min = Double.MAX_VALUE;	
		for (int i=this.getStatSize()-1; i>=0; i--) {
			double sp = getStat(i).getSelfP();
			if (sp < min)
				min = sp;
		}
		return min;
	}
	
	public double getMaxSelfP() {
		double max = 1.0;	
		for (int i=this.getStatSize()-1; i>=0; i--) {
			double sp = getStat(i).getSelfP();
			if (sp > max)
				max = sp;
		}
		return max;
	}
	
	public CRegionStat getRegionStat() {
		return stats.get(0);
	}
	
	public int getRecursionDepth() {
		return stats.size();
	}

	@Override
	public long getTotalWork() {
		return this.getStat(0).getTotalWork();		
	}

	@Override
	public long getAvgWork() {
		return this.getStat(0).getAvgWork();
	}
	
	public String getStatString() {		
		String stats = null;
		
		stats = String.format("sp = %5.2f [%5.2f - %5.2f] count = %d statSize = %d",
				getSelfP(), getMinSelfP(), getMaxSelfP(), getInstanceCount(), this.getStatSize());

		if (this.getRegionType() == CRecursiveType.REC_SINK) {
			stats += String.format(" rtarget = %d", getRecursionTarget().id);
		}
		
		return stats;
	}
}

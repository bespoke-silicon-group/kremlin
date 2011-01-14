package target;

import java.util.ArrayList;
import java.util.List;
import pprof.SRegionInfo;


/**
 * Omp Performance Estimator
 * @author dhjeon
 *
 *	Parallel Time t is modeled as:
 *   Tn = Work + Barrier + Sched;
 *   
 *  Work = W / n;
 *  Barrier = B * n;
 *  Sched = S * u;
 *  
 *  
 *  Tn is minimized when,
 *      n = sqrt (W/B) 
 *       
 *  Tn,u = W/n + B*n + S*u
 *  
 *  
 */

public class TargetOmp implements ITarget {
	double costBarrier = 2500;
	double costSched = 300;
	
	int getOptimalCore(int maxCore, double work, double barrierCost) {
		double n = Math.sqrt(work / barrierCost);
		
		if (maxCore < n)
			return maxCore;
		
		if (n < 1.0)
			return 1;
		
		return (int)n;		
	}
	
	
	
	public double getBarrierCost(double nCore) {
		return nCore * costBarrier;
	}
	
	public double getSchedCost(double nCore, double nUnit) {
		return nUnit * costSched;
	}
	
	public double getWorkCost(double work, double n) {
		return work / n;
	}
	
	public double getSpeedup(int nCore, double work, double nUnit, double barrierCost, double schedCost) {
		double serialTime = work;
		double parallelTime= (work / nCore) + getBarrierCost(nCore) + getSchedCost(nCore, nUnit);
		double speedup = serialTime / parallelTime;
		
		return speedup;
	
	}
	
		
	private SpeedupRecord getSPRecord(SRegionInfo info, int maxCore) {
		double sp = info.getSelfParallelism();
		if (maxCore < sp)
			sp = maxCore;
		return new SpeedupRecord("SP", (int)sp, sp);
	}
	
	
	private SpeedupRecord getNoBarrierSchedBest(SRegionInfo info, int maxCore) {
		double sp = info.getSelfParallelism();
		double iter = info.getAvgIteration();
		double work = info.getAvgWork();
		double barrierCost = 0.0;
		int optCore  = getOptimalCore(maxCore, work, barrierCost);
		double speedup = getSpeedup(optCore, work, optCore, barrierCost, this.costSched);
		return new SpeedupRecord("No Barrier, SchedBest", optCore, speedup);
	}
	
	private SpeedupRecord getNoBarrierSchedWorst(SRegionInfo info, int maxCore) {		
		double iter = info.getAvgIteration();
		double work = info.getAvgWork();
		double barrierCost = 0.0;
		int optCore  = getOptimalCore(maxCore, work, barrierCost);
		double speedup = getSpeedup(optCore, work, iter, barrierCost, this.costSched);
		return new SpeedupRecord("No Barrier, SchedWorst", optCore, speedup);
	}
	
	private SpeedupRecord getBarrierSchedBest(SRegionInfo info, int maxCore) {		
		double iter = info.getAvgIteration();
		
		double work = info.getAvgWork();
		double barrierCost = this.costBarrier;			
		int optCore  = getOptimalCore(maxCore, work, barrierCost);
		if (iter > optCore)
			iter = optCore;
		
		optCore = maxCore;
			
		double speedup = getSpeedup(optCore, work, maxCore, barrierCost, this.costSched);
		return new SpeedupRecord("Barrier, Sched Best", optCore, speedup);
	}
	
	private SpeedupRecord getBarrierSchedWorst(SRegionInfo info, int maxCore) {		
		double iter = info.getAvgIteration();
		double work = info.getAvgWork();
		double barrierCost = this.costBarrier;
		int optCore  = getOptimalCore(maxCore, work, barrierCost);
		double speedup = getSpeedup(optCore, work, iter, barrierCost, this.costSched);
		return new SpeedupRecord("Barrier, Sched Worst", optCore, speedup);
	}
	
	public List<SpeedupRecord> getSpeedupList(SRegionInfo info, int maxCore) {
		List<SpeedupRecord> ret = new ArrayList<SpeedupRecord>();
		ret.add(getSPRecord(info, maxCore));
		//ret.add(getNoBarrierSchedBest(info, maxCore));
		//ret.add(getNoBarrierSchedWorst(info, maxCore));
		ret.add(getBarrierSchedBest(info, maxCore));
		//ret.add(getBarrierSchedWorst(info, maxCore));		
		return ret;
	}

}

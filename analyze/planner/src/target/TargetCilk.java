package target;

import java.util.*;

import pprof.SRegionInfo;

/**
 * Cilk Performance Estimator
 * @author dhjeon
 *
 *	Parallel Time t is modeled as:
 *   Tn = Work + Barrier + Sched;
 *   
 *  Work = W / n;
 *  Barrier = B * log n;
 *  Sched = S * log n * log u;
 *  u = iter / min(512, grainsize); // default mode
 *  grainsize = iter / (8 * n);
 *  
 *  u = 8 * n (iter count is small) or iter / 512
 *  
 *  Tn is minimized when,
 *      n = (W * log2) / (B + S log u) 
 *       (log 2 is approximately 0.693)
 *  
 *  
 *  
 */

public class TargetCilk implements ITarget {
	double costBarrier = 10000;
	double costSched = 10000;	 

	int getOptimalCore(int maxCore, double work, double nUnit, double barrierCost, double schedCost) {
		double n = (work * Math.log(2.0)) / (barrierCost + schedCost * Math.log(nUnit) / Math.log(2));
		if (maxCore < n)
			return maxCore;
		if (n < 1.0)
			return 1;
		
		return (int)n;
		
	}
	
	int getDefaultGrainSize(SRegionInfo info, int nCore) {
		int iter = (int)info.getAvgIteration();
		int ret = iter / (8 * nCore);
		if (ret > 512)
			return 512;
		else
			return ret;
	}
	
	public double getBarrierCost(double nCore) {
		return Math.log(nCore) * costBarrier;
	}
	
	public double getSchedCost(double nCore, double nUnit) {
		//return Math.log(nCore) * Math.log(nUnit) * costSched;
		return nUnit / nCore * 200;
	}
	
	public double getWorkCost(double work, double n) {
		return work / n;
	}
	
	public double getSpeedup(int nCore, double work, double nUnit, double barrierCost, double schedCost) {
		double serialTime = work;
		double parallelTime= (work / nCore) + getBarrierCost(nCore) + getSchedCost(nCore, nUnit);
		double speedup = serialTime / parallelTime;
		
		//if (nCore == 1.0)
		//	return 1.0;
		
		//if (speedup < 1.0)
		//	return 1.0;
		//else
			return speedup;
	
	}
	
	
	 

	
	private SpeedupRecord getSPRecord(SRegionInfo info, int maxCore) {
		double sp = info.getSelfParallelism();
		if (maxCore < sp)
			sp = maxCore;
		return new SpeedupRecord("SP", (int)sp, sp);
	}
	/*
	private SpeedupRecord getBarrierNoSched(SRegionInfo info, int maxCore) {
		double work = info.getAvgWork();
		double schedCost = 0.0;
		int optCore  = getOptimalCore(maxCore, work, 0, this.costBarrier, schedCost);
		double speedup = getSpeedup(optCore, work, 1, this.costBarrier, schedCost);			
		
		return new SpeedupRecord("Barrier, No Sched", optCore, speedup);
	}*/
	

	private SpeedupRecord getBarrierSchedBest(SRegionInfo info, int maxCore) {		
		double iter = info.getAvgIteration();
		
		double work = info.getAvgWork();
		double barrierCost = this.costBarrier;			
		int optCore  = getOptimalCore(maxCore, work, iter, barrierCost, this.costSched);
		optCore = maxCore;
		//if (iter > optCore)
		//	iter = optCore;
		double nUnit = optCore;		
			
		double speedup = getSpeedup(optCore, work, nUnit, barrierCost, this.costSched);
		return new SpeedupRecord("Barrier, Sched Best", optCore, speedup);
	}
	
	private SpeedupRecord getBarrierSchedDefault(SRegionInfo info, int maxCore) {		
		double iter = info.getAvgIteration();
		
		double work = info.getAvgWork();
		double barrierCost = this.costBarrier;			
		int optCore  = getOptimalCore(maxCore, work, iter, barrierCost, this.costSched);
		optCore = maxCore;
		//if (iter > optCore)
		//	iter = optCore;
		double nUnit = iter / this.getDefaultGrainSize(info, maxCore);
		
		
			
		double speedup = getSpeedup(optCore, work, nUnit, barrierCost, this.costSched);
		return new SpeedupRecord("Barrier, Sched Default", optCore, speedup);
	}
	
	private SpeedupRecord getBarrierSchedWorst(SRegionInfo info, int maxCore) {		
		double iter = info.getAvgIteration();
		double work = info.getAvgWork();
		double barrierCost = this.costBarrier;
		//int optCore  = getOptimalCore(maxCore, work, iter, barrierCost, this.costSched);
		int optCore = maxCore;
		double speedup = getSpeedup(optCore, work, iter, barrierCost, this.costSched);
		return new SpeedupRecord("Barrier, Sched Worst", optCore, speedup);
	}

	/*
	public List<SpeedupRecord> getSpeedupList(SRegionInfo info) {
		List<SpeedupRecord> ret = new ArrayList<SpeedupRecord>();
		ret.add(getSPRecord(info));
		ret.add(getBarrierNoSched(info));
		ret.add(getNoBarrierSchedBest(info));
		ret.add(getNoBarrierSchedWorst(info));
		ret.add(getBarrierSchedBest(info));
		ret.add(getBarrierSchedWorst(info));
		
		return ret;
	}*/
	
	public List<SpeedupRecord> getSpeedupList(SRegionInfo info, int maxCore) {
		List<SpeedupRecord> ret = new ArrayList<SpeedupRecord>();
		ret.add(getSPRecord(info, maxCore));
		//ret.add(getBarrierNoSched(info, maxCore));
		
		ret.add(getBarrierSchedBest(info, maxCore));
		ret.add(getBarrierSchedDefault(info, maxCore));
		ret.add(getBarrierSchedWorst(info, maxCore));		
		return ret;
	}
}

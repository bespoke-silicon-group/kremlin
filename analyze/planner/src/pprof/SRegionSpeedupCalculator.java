package pprof;

import java.util.*;

import pyrplan.ParameterSet;

import target.*;
public class SRegionSpeedupCalculator {
	public enum ScaleMode {LINEAR, SQRT};
	public enum LimitFactor {SP, GRANULARITY, BANDWIDTH, ALL};
	
	ScaleMode scale;
	LimitFactor factor;
	double satBw;
	double minWorkChunk;
	TargetCilk target = new TargetCilk();
	
	public SRegionSpeedupCalculator(ScaleMode scale, LimitFactor factor, double minWorkChunk, double satBw) {
		this.scale = scale;
		this.factor = factor;
		this.satBw = satBw;
		this.minWorkChunk = minWorkChunk;
	}
	
	public int getMaxProfitableChunkNumber(SRegionInfo info) {				
		int maxCore = (int) (Math.ceil(info.getAvgIteration() / getMinChunkSize(info)));
		return maxCore;
	}
	
	public int getMinChunkSize(SRegionInfo info) {
		int k = (int) Math.ceil(this.minWorkChunk / info.getAvgIterWork());
		
		if (info.getAvgIterWork() > 0)
			assert(k >= 1);
		return k;
	}
	
	public LimitFactor getLimitFactor(SRegionInfo info) {
		
		/*
		double speedupSP = info.getSelfParallelism();			
		//double speedupGranularity = getMaxProfitableChunkNumber(info);
		//double speedupBandwidth = info.getBandwidthMaxCore(satBw);
		double speedupGranularity = target.getMaxSpeedup(info.getAvgWork(), info.getAvgIteration());
		double speedupBandwidth = target.getMinSpeedup(info.getAvgWork(), info.getAvgIteration());
		
		if (speedupSP <= speedupGranularity) {
			if (speedupSP <= speedupBandwidth)
				return LimitFactor.SP;
			else
				return LimitFactor.BANDWIDTH;
		} else {
			if (speedupGranularity <= speedupBandwidth)
				return LimitFactor.GRANULARITY;
			else
				return LimitFactor.BANDWIDTH;
		}	*/
		return LimitFactor.SP;
		
	}
	
	public double getRegionSpeedup(SRegionInfo info, LimitFactor factor) {
		return info.getSelfParallelism();
		/*if (factor == LimitFactor.SP)
			return info.getSelfParallelism();
		
		else if (factor == LimitFactor.GRANULARITY)
			return target.getMaxSpeedup(info.getAvgWork(), info.getAvgIteration());
		
		else if (factor == LimitFactor.BANDWIDTH)
			return target.getMinSpeedup(info.getAvgWork(), info.getAvgIteration());
			//return info.getBandwidthMaxCore(satBw);
		else {
			assert(false);
			return 0.0; 
		}*/
			
	}
	
	public int getOptimalCoreSize(SRegionInfo info, LimitFactor factor) {
		return (int)info.getSelfParallelism();
		/*
		if (factor == LimitFactor.SP)
			return (int)info.getSelfParallelism();
		else if (factor == LimitFactor.GRANULARITY)
			return (int)target.getMaxOptimalCoreNum(info.getAvgWork(), info.getAvgIteration());
		
		else if (factor == LimitFactor.BANDWIDTH)
			return (int)target.getMinOptimalCoreNum(info.getAvgWork(), info.getAvgIteration());
			
			//return info.getBandwidthMaxCore(satBw);
		else {
			assert(false);
			return 1; 
		}*/
	}
	
	public double getRegionSpeedup(SRegionInfo info) {
		
		double speedupSP, speedupGranularity;
		speedupSP = info.getSelfParallelism();
		return speedupSP;
		/*
		//speedupGranularity = getMaxProfitableChunkNumber(info);
		speedupGranularity = target.getMaxSpeedup(info.getAvgWork(), info.getAvgIteration());
		
		
		if (speedupSP <= speedupGranularity)
			speedupGranularity = speedupSP;
		
		//double speedupBandwidth = info.getBandwidthMaxCore(satBw);
		double speedupBandwidth = target.getMinSpeedup(info.getAvgWork(), info.getAvgIteration());		
		assert(speedupSP >= speedupGranularity);
		
		double retSpeedup;
		if (factor == LimitFactor.SP)
			retSpeedup = speedupSP;
		else if (factor == LimitFactor.GRANULARITY)
			retSpeedup = speedupGranularity;
		else if (factor == LimitFactor.BANDWIDTH)
			retSpeedup = speedupBandwidth;
		else
			retSpeedup = (speedupGranularity < speedupBandwidth) ? speedupGranularity : speedupBandwidth;
		
		//System.out.printf("\t%.2f, %.2f, %.2f\n", speedupSP, speedupGranularity, speedupBandwidth);
			
		double ret = retSpeedup;
		if (this.scale == ScaleMode.SQRT)
			ret = Math.sqrt(ret);
				
		return ret;*/
	}
	
	public double getRegionSpeedup(SRegionInfo info, int core) {
		double ret = (this.scale == ScaleMode.SQRT) ? Math.sqrt((double)core) : (double)core;
		double speedup = getRegionSpeedup(info);
		if (core < 0)
			return speedup;
		return (ret < speedup) ? ret : speedup;			
	}
		
	public double getAppTimeReduction(SRegionInfo info, SRegionInfo root) {
		double regionSpeedup = getRegionSpeedup(info);
		
		double coverage = ((double)info.getTotalWork() / (double)root.getTotalWork()) * 100.0;
		//return info.getCoverage() - info.getCoverage() / getRegionSpeedup(info);
		//System.out.println("cov: " + coverage + " regionSpeedup: " + regionSpeedup);
		return coverage - coverage / regionSpeedup;
	}
		
	
	public double getAppTimeReduction(Set<SRegionInfo> set) {
		double total = 0.0;
		for (SRegionInfo each : set) {
			double reduction = each.getCoverage() - each.getCoverage() / getRegionSpeedup(each);
			total += reduction;
		}
		return total;
	}
	
	public double getAppTimeReduction(Set<SRegionInfo> set, int core) {
		double total = 0.0;
		for (SRegionInfo each : set) {
			double reduction = each.getCoverage() - each.getCoverage() / getRegionSpeedup(each, core);
			total += reduction;
		}
		return total;
	}
	
	public double getAppSpeedup(Set<SRegionInfo> set) {
		double reduction = getAppTimeReduction(set);
		return 1.00 / (1.00 - reduction / 100.0);
	}
	
	public double getAppSpeedup(Set<SRegionInfo> set, int core) {
		double reduction = getAppTimeReduction(set, core);
		return 1.00 / (1.00 - reduction / 100.0);
	}
	
}

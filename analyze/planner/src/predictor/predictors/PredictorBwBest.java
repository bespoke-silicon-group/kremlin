package predictor.predictors;

import predictor.PElement;
import pprof.*;

public class PredictorBwBest implements ISpeedupPredictor {
	double bwMByteSec;
	double clockMHz;
	double cacheMByte;
	
	public PredictorBwBest(double bwMByteSec, double clockMHz, double cacheMByte) {
		this.bwMByteSec = bwMByteSec;
		this.clockMHz = clockMHz;
		this.cacheMByte = cacheMByte;
		//this.cacheMByte = 0.0;
	}
	
	@Override
	public String getDesc() {
		// TODO Auto-generated method stub
		//return String.format("BwBest [BW=%.2f MB/s]", this.bwMByteSec);
		return "BwBest";
	}
/*
	@Override
	public double predictSpeedup(PElement element) {
		SRegionInfo info = element.getSRegionInfo();		
		double requiredMByte = ((info.getAvgMemReadCnt() * 4) - cacheMByte * 1024 * 1024) / (1024 * 1024);
		if (requiredMByte < 0.0)
			requiredMByte = 0.00001;
		
		double minTime = requiredMByte / this.bwMByteSec;
		double cycleTime = (element.getSerialTime() / (1024*1024.0)) / clockMHz;
		double speedup = cycleTime / minTime;
		if (speedup > 1024.0)
			speedup = 1024.0;		
		
		assert(speedup > 0.0);
		return speedup;
	}*/
	
	@Override
	public double predictSpeedup(PElement element) {		
		SRegionInfo info = element.getSRegionInfo();
		double requiredCnt = info.getAvgMemReadCnt() + info.getAvgMemReadCnt();
		double requiredMByteSec = ((requiredCnt * 4) - cacheMByte * 1024 * 1024) / (1024 * 1024);
		if (requiredMByteSec < 0.0)
			requiredMByteSec = 0.00001;
		
		double minTime = requiredMByteSec / this.bwMByteSec;
		double cycleTime = (info.getAvgWork() / (1024*1024.0)) / clockMHz;
		double speedup = cycleTime / minTime;
		
		return (speedup > 1024.0) ? 1024.0 : speedup; 
	}


}

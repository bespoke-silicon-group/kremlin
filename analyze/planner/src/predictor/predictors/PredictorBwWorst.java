package predictor.predictors;

import predictor.PElement;
import pprof.*;
public class PredictorBwWorst implements ISpeedupPredictor {
	double bwMByteSec;
	double clockMHz;
	
	public PredictorBwWorst(double bwMByteSec, double clockMHz) {
		this.bwMByteSec = bwMByteSec;
		this.clockMHz = clockMHz;
	}
	
	@Override
	public String getDesc() {
		// TODO Auto-generated method stub
		//return String.format("BwWorst [BW=%.2f MB/s]", this.bwMByteSec);
		return "BwWorst";
	}

	@Override
	public double predictSpeedup(PElement element) {		
		SRegionInfo info = element.getSRegionInfo();
		double requiredCnt = info.getAvgMemReadCnt() + info.getAvgMemReadCnt();
		double requiredMByteSec = (requiredCnt * 4) / (1024 * 1024);
		double minTime = requiredMByteSec / this.bwMByteSec;
		double cycleTime = (info.getAvgWork() / (1024*1024.0)) / clockMHz;
		double speedup = cycleTime / minTime;
		
		return (speedup > 1024.0) ? 1024.0 : speedup; 
	}

}

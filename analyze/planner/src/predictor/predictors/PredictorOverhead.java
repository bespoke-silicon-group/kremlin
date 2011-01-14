package predictor.predictors;

import predictor.PElement;
import pprof.*;

public class PredictorOverhead implements ISpeedupPredictor {
	int maxCore;
	public PredictorOverhead(int maxCore) {
		this.maxCore = maxCore;
	}
	
	
	@Override
	public String getDesc() {		
		return "Overhead";
	}
	
	int getParallelOverhead(int nCore) {
		return nCore * 1024;
	}
	
	double predictParallelTime(double work, int nCore) {
		return work / nCore + getParallelOverhead(nCore);
	}

	@Override
	public double predictSpeedup(PElement element) {
		SRegionInfo info = element.getSRegionInfo();
		double work = info.getAvgWork();
		double min = work;
		for (int core=2; core<=maxCore; core++) {
			double predicted = predictParallelTime(work, core);
			if (predicted < min)
				min = predicted;
		}		
		return work / min;
	}

}

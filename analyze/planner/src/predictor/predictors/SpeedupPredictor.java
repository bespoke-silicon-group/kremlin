package predictor.predictors;

import java.util.*;
import pprof.*;
import predictor.PElement;
import predictor.PredictUnit;
import pyrplan.*;
import predictor.*;

public class SpeedupPredictor {
	List<ISpeedupPredictor> predictorList;
	
	public SpeedupPredictor(List<ISpeedupPredictor> list) {
		//this.predictorList = new ArrayList<ISpeedupPredictor>();
		this.predictorList = list;
		//predictorList.add(new PredictorCore(16));		
		//predictorList.add(new PredictorSp());
		//predictorList.add(new PredictorOverhead());
		//predictorList.add(new PredictorBwBest(2048, 2660, 2));
		//predictorList.add(new PredictorBwWorst(2048, 2660));				
	}
	
	public PredictUnit predictSpeedup(PElement element) {
		double minSpeedup = Double.MAX_VALUE;
		String minPredictor = null;
		List<Double> valueList = new ArrayList<Double>();
		for (ISpeedupPredictor each : predictorList) {
			double speedup = each.predictSpeedup(element);
			valueList.add(speedup);
			if (speedup < minSpeedup) {
				minSpeedup = speedup;
				minPredictor = each.getDesc();
			}
		}	
		
		return new PredictUnit(element.getSRegionInfo(), minSpeedup, minPredictor, predictorList, valueList);
	}
	
}

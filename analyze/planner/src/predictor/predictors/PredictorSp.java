package predictor.predictors;

import predictor.PElement;

public class PredictorSp implements ISpeedupPredictor {

	@Override
	public String getDesc() {
		// TODO Auto-generated method stub
		return "Predictor [SP]";
	}

	@Override
	public double predictSpeedup(PElement element) {		
		return element.getSRegionInfo().getSelfParallelism();
	}
}

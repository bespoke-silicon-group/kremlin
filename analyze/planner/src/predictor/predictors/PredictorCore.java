package predictor.predictors;

import predictor.PElement;

public class PredictorCore implements ISpeedupPredictor {
	int nCore;
	public PredictorCore(int nCore) {
		this.nCore = nCore;
	}	
	
	@Override
	public String getDesc() {		
		//return String.format("Predictor [Core = %d]", nCore);
		return "Core";
	}

	@Override
	public double predictSpeedup(PElement element) {		
		return nCore;
	}
}

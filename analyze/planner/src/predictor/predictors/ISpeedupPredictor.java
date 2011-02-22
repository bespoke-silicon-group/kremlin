package predictor.predictors;
import planner.*;
import pprof.*;
import predictor.PElement;

public interface ISpeedupPredictor {
	public double predictSpeedup(PElement element);	
	public String getDesc();
}

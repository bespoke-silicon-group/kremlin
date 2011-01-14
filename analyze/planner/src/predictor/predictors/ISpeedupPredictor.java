package predictor.predictors;
import pprof.*;
import predictor.PElement;
import pyrplan.*;

public interface ISpeedupPredictor {
	public double predictSpeedup(PElement element);	
	public String getDesc();
}

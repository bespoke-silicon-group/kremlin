package predictor.topdown;

import planner.*;
public class InfoStatus {
	double maxCore;
	double allocatedCore;
	double speedup;
		
	InfoStatus(double maxCore) {
		this.maxCore = maxCore;
		this.allocatedCore = 1.0;
	}
	
	void setSpeedup(double speedup) {
		this.speedup = speedup;
	}
	
}

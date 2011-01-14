package predictor;

import java.util.*;
import pprof.*;
import predictor.predictors.ISpeedupPredictor;

public class PredictUnit {
	public PredictUnit(SRegionInfo info, double value, String desc, List<ISpeedupPredictor> predictors, List<Double> values) {
		this.info = info;
		this.speedup = value;
		this.effectivePredictor = desc;
		this.predictorList = predictors;
		this.speedupList = values;
	}
	
	SRegionInfo info;
	String effectivePredictor;
	double speedup;
	
	List<ISpeedupPredictor> predictorList;
	List<Double> speedupList;
	String valueListToString() {
		StringBuffer buffer = new StringBuffer("[");
		for (Double each : speedupList) {
			buffer.append(String.format("%.2f\t", each));
		}
		buffer.append("]");
		return buffer.toString();
	}
	
	public String toString() {
		StringBuffer buffer = new StringBuffer();
		buffer.append(String.format("[%4d - %4d]", info.getSRegion().getStartLine(), info.getSRegion().getEndLine()));
		buffer.append(String.format("\t%.2f (%10s)", speedup, effectivePredictor));
		buffer.append(String.format("\t%s ", valueListToString()));
		return buffer.toString();
	}
	
	public String tableString() {
		StringBuffer buffer = new StringBuffer();
		buffer.append(String.format("%d\t%.2f\t%s\t", info.getSRegion().getStartLine(),speedup, effectivePredictor));
		for (Double each : speedupList) {
			buffer.append(String.format("%.2f\t", each));
		}
		return buffer.toString();	
	}
}

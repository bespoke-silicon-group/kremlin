package predictor;

import predictor.predictors.*;
import pprof.*;
import java.util.*;

public class ParallelPredictor {
	RegionTimeTree tree;
	SpeedupPredictor predictor;
	
	public ParallelPredictor(RegionTimeTree tree, List<ISpeedupPredictor> list) {
		this.tree = tree;
		this.predictor = new SpeedupPredictor(list);
	}
	
	public void printRegionPrediction() {
		Map<String, Integer> map = new HashMap<String, Integer>();
		for (int i=1; i<tree.getTreeSize(); i++) {
			PElement element = tree.getTreeElement(i);			
			PredictUnit unit = predictor.predictSpeedup(element);
			element.parallelize(unit.speedup);
			
			//System.out.println(unit.speedup);
			if (map.containsKey(unit.effectivePredictor)) {
				int value = map.get(unit.effectivePredictor) + 1;
				map.put(unit.effectivePredictor, value);
			} else {
				map.put(unit.effectivePredictor, 1);
			}
			System.out.println(unit.tableString());
		}		
		System.out.println(map);
	}
	
	public void predict() {
		Map<String, Integer> map = new HashMap<String, Integer>();
		for (int i=1; i<tree.getTreeSize(); i++) {
			PElement element = tree.getTreeElement(i);
			SRegionInfo info = element.info;
			PredictUnit unit = predictor.predictSpeedup(element);
			element.parallelize(unit.speedup);
			
			//System.out.println(unit.speedup);
			if (map.containsKey(unit.effectivePredictor)) {
				int value = map.get(unit.effectivePredictor) + 1;
				map.put(unit.effectivePredictor, value);
			} else {
				map.put(unit.effectivePredictor, 1);
			}
			//System.out.println(unit.tableString());
		}		
		//System.out.printf("App speedup = %.2f\n", getAppSpeedup());
		//System.out.println(map);
	}	
	
	public double getAppSpeedup() {
		predict();
		long timeSaved = 0;
		long totalWork = 0;
		PElement root = tree.getTreeElement(0);
		
		for (int i=1; i<tree.getTreeSize(); i++) {			
			PElement element = tree.getTreeElement(i);
			timeSaved += element.serialTime - element.parallelTime;
			totalWork += element.serialTime;
			//System.out.println(element);
		}
		/*System.out.println(root);
		System.out.println(totalWork);
		System.out.println(timeSaved);*/
		assert(totalWork <= root.serialTime);
		long parallelTime = root.serialTime - timeSaved;
		return root.serialTime / (double)parallelTime;
	}
}

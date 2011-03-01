package planner;

import java.util.*;
import pprof.*;

public class RawPlanner extends CDPPlanner {
	public RawPlanner(CRegionManager analyzer, Target target) {		
		super(analyzer, target);
	}
	
	protected double getParallelTime(CRegion region) {
		double spSpeedup = (this.maxCore < region.getSelfParallelism()) ? maxCore : region.getSelfParallelism();
		double parallelTime = region.getAvgWork() / spSpeedup + overhead;
		return parallelTime;
	}
}

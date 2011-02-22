package planner;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.*;

import pprof.*;

public abstract class Planner {
	protected SRegionInfoAnalyzer analyzer;
	List<SRegionInfo> lastPlan;
	
	public Planner(SRegionInfoAnalyzer analyzer) {
		this.analyzer = analyzer;
		this.lastPlan = null;
	}
	
	protected void setPlan(List<SRegionInfo> plan) {
		this.lastPlan = plan;
	}	
	
	protected List<SRegionInfo> getPlan() {
		assert(this.lastPlan != null);
		return this.lastPlan;
	}
	
	public void emitParallelRegions(String file) {
		Set<Integer> retired = new HashSet<Integer>();
		
		try {
			BufferedWriter output = new BufferedWriter(new FileWriter(file));
			List<SRegionInfo> list = this.getPlan();
			for (SRegionInfo each : list) {
				System.out.println(each);
				//String str = each.getSRegion().getStartLine()
				int line = (each.getSRegion().getStartLine());
				if (!retired.contains(line)) {
					output.append(line + "\n");
					retired.add(line);
				}
			}
			output.close();
			
		} catch(Exception e) {
			assert(false);
		}
	}
	
}

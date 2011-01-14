package pyrplan.analyzer;

import pprof.SRegionInfo;
import pprof.SRegionInfoAnalyzer;
import pprof.URegionManager;
import pyrplan.RecList;
import pyrplan.RecList.RecUnit;

public class FlatProfiler {
	URegionManager manager;
	RecList list;
	SRegionInfoAnalyzer analyzer;
	
	FlatProfiler(RecList list, URegionManager manager) {
		this.list = list;
		this.manager = manager; 
		this.analyzer = new SRegionInfoAnalyzer(manager);
	}
	
	void dump() {
		for (int i=0; i<list.size(); i++) {
			RecList.RecUnit unit = list.get(i);
			SRegionInfo info = analyzer.getSRegionInfo(unit.getRegion());
			System.out.printf("[%d] %s\n\n", i, info);
		}
	}
	
}

package pyrplan;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.*;

import pprof.*;

public class ParallelFileReader {
	static List<SRegionInfo> readParallelFile(String file, SRegionInfoAnalyzer analyzer) {
		List<SRegionInfo> ret = new ArrayList<SRegionInfo>();		
		
		
		if (file == null) {
			return new ArrayList<SRegionInfo>();
		}
		
		List<Long> lineList = Util.readFile(file);
		
		for (SRegionInfo info : analyzer.getSRegionInfoSet()) {
			SRegion region = info.getSRegion();
			//if (lineList.contains(region.getID())) {
			//	System.out.println("parallel line: " + region.getStartLine());
			//} else if (region.getType() == RegionType.LOOP){
			//	ret.add(info);
			//}
			if (region.getType() == RegionType.LOOP)
				ret.add(info);
		}
		//assert(false);
		return ret;		
	}
}

package runner;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.*;

import pprof.*;

public class PyrComparator {
	List<SRegionInfo> reference;
	public PyrComparator(List<SRegionInfo> reference) {
		this.reference = reference;
	}
	
	String toSRegionString(SRegion region) {
		String func = region.getFuncName();
		if (func.length() > 10)
			func = func.substring(0, 9) + "~";
		
		String module = region.getModule();
		if (module.length() > 10)
			module = module.substring(0, 9) + "~";
		
		return String.format("%15s:%15s [%4d - %4d] %s", 
				module + ".c", func, region.getStartLine(), region.getEndLine(), region.getType());
	}
	
	String formatInfoEntry(SRegionInfo info) {
		StringBuffer buffer = new StringBuffer();				
		boolean exploited = info.isExploited();
		
		String numbers = String.format("%d\t%10.2f%10.2f %10.2f %10.2f %10.2f %10.2f %15.2f %10.2f %10.2f %10s",
				info.getInstanceCount(), info.getSelfSpeedup(), info.getAvgIteration(), info.getSelfParallelism(), 
				info.getCoverage(), info.getNSP(),   
				info.getWorkDeviationPercent(), info.getAvgWork(), info.getAvgIterWork(),  
				info.getTotalParallelism(), exploited				 
				);
		buffer.append(numbers);
		return buffer.toString();
	}
	
	void evalute(List<SRegionInfo> list, String desc) {
		long totalCnt = list.size();
		long hitCnt = 0;
		double accSpeedup = 1.0;
		
		List<SRegionInfo> diffList = new ArrayList<SRegionInfo>(reference);
		diffList.removeAll(list);
		
		for (SRegionInfo each : list) {
			if (reference.contains(each)) {
				hitCnt++;
			}
		}
		
		double hitRatio = (double)hitCnt / list.size() * 100.0;
		System.out.printf("Compare %s against ref:  \t %.2f%% (%d / %d) matched\n", 
				desc, hitRatio, hitCnt, list.size());
		
	}
	
	public void emitAnalysis(List<SRegionInfo> list, String file) {
		long totalCnt = list.size();
		long hitCnt = 0;
		double accSpeedup = 1.0;
		
		List<SRegionInfo> diffList = new ArrayList<SRegionInfo>(reference);
		diffList.removeAll(list);
		
		Set<Integer> retired = new HashSet<Integer>();
		
		long i = 1;
		
		try {
			BufferedWriter output = new BufferedWriter(new FileWriter(file));
			for (SRegionInfo info : list) {
				//if (reference.contains(info)) {
					if (reference.contains(info)) 
						hitCnt++;
					//SRegionInfo info = analyzer.getSRegionInfo(region);
					RegionStatus status = info.getRegionStatus();
					SRegion region = info.getSRegion();
					boolean exploitStatus = info.isExploited();
					//double speedup = 1.0 / unit.getSpeedup();
					//accSpeedup = accSpeedup * speedup;

					int startLine = region.getStartLine();
					
					//if (retired.contains(startLine))
					//	continue;
					//else {
					
						String str = String.format("[%2d, %5.2f, %5s] %s %s", 
								i++, accSpeedup, exploitStatus, 
								toSRegionString(region), formatInfoEntry(info));
						//System.out.println(str);
						output.append(str + "\n");
						retired.add(startLine);
					//}

				//}
			}

			//System.out.println("\n\nDiff List\n");
			output.append("\nIn Reference, but Not Included\n");
			
			for (SRegionInfo info : diffList) {
				RegionStatus status = info.getRegionStatus();
				SRegion region = info.getSRegion();
				boolean exploitStatus = info.isExploited();
				//double speedup = 1.0 / unit.getSpeedup();
				//accSpeedup = accSpeedup * speedup;
				
				int startLine = region.getStartLine();
				
				if (retired.contains(startLine))
					continue;
				else {
					String str = String.format("[%2d, %5.2f, %5s] %s %s", 
							i++, accSpeedup, exploitStatus, 
							toSRegionString(region), formatInfoEntry(info));
					//System.out.println(str);
					output.append(str + "\n");
					retired.add(startLine);
				}

			}
			output.close();
			
		} catch (Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	
}

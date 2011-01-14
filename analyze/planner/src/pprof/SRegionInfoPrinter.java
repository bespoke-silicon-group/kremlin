package pprof;
import java.io.*;
import java.util.*;

public class SRegionInfoPrinter {
	public static String toHeaderString() {
		StringBuffer buffer = new StringBuffer();
		buffer.append("===========================================================" +
				"===========================================================================================" +
				"===================\n");
		
		buffer.append(String.format("%s %10s %10s %10s %15s  %10s  %10s\n", 
				SRegion.toHeaderString(), "(speedup, tReduction)", "tCoverage(%)", 
				"(avgIter   ,  sp    )", "avgIterWork", "read(all, line)", "write(all, line)"
				));
				
		
		buffer.append("===========================================================" +
				"============================================================================================" +
				"===================\n");
		return buffer.toString();
	}
	
	public static String formatInfoEntry(SRegionInfo info) {
		StringBuffer buffer = new StringBuffer();
		
		String numbers = String.format("(%.2f, %5.2f)\t%10.2f\t(%10.2f, %10.2f)\t%10.2f\t(%5.2f, %5.2f, %5.2f)\t(%5.2f, %5.2f)",
				info.getSelfSpeedup(), info.getTimeReduction(), info.getCoverage(), 
				info.getAvgIteration(), info.getSelfParallelism(), info.getAvgIterWork(),  
				//info.getMaxProfitableChunkNumber(), 
				info.getAvgMemReadCnt(), info.getMemReadRatio()*100.0, info.getMemReadLineRatio()*100.0,
				info.getMemWriteRatio()*100.0, info.getMemWriteLineRatio()*100.0
				);				
				
		buffer.append(numbers);
		return buffer.toString();
	}
	
	/*
	public static String formatInfoEntryOld(SRegionInfo info) {
		StringBuffer buffer = new StringBuffer();				
		boolean exploited = info.isExploited();		
		
		String numbers = String.format("(%5s)\t%10.2f %10.2f %10.2f %10.2f %10.2f %15.2f %10.2f (%5.2f %5.2f) %10s",
				info.isLeaf(), info.getSelfSpeedup(), info.getSelfParallelism(), info.getCoverage(), info.getNSP(),   
				info.getAvgCP(), info.getAvgWork(), info.getTotalParallelism(), info.getMemReadRatio(), info.getMemWriteRatio(), exploited				 
				);
		buffer.append(numbers);
		return buffer.toString();
	}*/
	
	public static String toEntryString(SRegionInfo info) {
		String ret = String.format("%30s %50s", info.getSRegion(), formatInfoEntry(info));
		return ret;
	}
	
	public static void dumpFile(List<SRegionInfo> list, String file) {
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(file));			
			//java.util.List<SRegionInfo> list = new ArrayList<SRegionInfo>();
			output.write(toHeaderString());
			for (SRegionInfo each : list) {
				output.write(toEntryString(each) + "\n");
			}
			output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
		}			
	}
}

package runner;

import java.io.*;
import java.util.*;
import pprof.*;

public class ProfileReporter {
	public static void report(String sFile, String dFile, String outFile) {
		SRegionManager sManager = new SRegionManager(new File(sFile), true);
		URegionManager dManager = new URegionManager(sManager, new File(dFile));
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		
		
		List<String> outList = new ArrayList<String>();
		printHeader(outList);
		SRegionInfo rootInfo = analyzer.getRootInfo();
		List<SRegionInfo> infoList = new ArrayList<SRegionInfo>(analyzer.getSRegionInfoSet());
		Collections.sort(infoList);
		
		for (SRegionInfo info : infoList) {			
			addEntry(outList, info, rootInfo);			
		}
		pyrplan.Util.writeFile(outList, outFile);
		System.out.println("File " + outFile + " emitted");
	}
	
	public static void addEntry(List<String> list, SRegionInfo info, SRegionInfo root) {
		SRegion region = info.getSRegion();
		double coverage = (double)info.getTotalWork() / root.getTotalWork() * 100.0;
		double covPerInstance = coverage / (double)info.getInstanceCount();
		list.add(String.format("%16x %15s [%4d-%4d] \t%5s\t%5.2f\t%5.2f\t%8d\t%5.2f", 
				region.getID(), region.getModule(), region.getStartLine(), region.getEndLine(), region.getType(),
				coverage, info.getSelfParallelism(), info.getInstanceCount(), covPerInstance 
				)
		);
				  
	}
	
	public static void printHeader(List<String> list) {
		list.add("===========================================================" +
		"===========================================================================================" +
		"===================");
		list.add(String.format("%10s %15s [start-end] \t%12s\t%6s %6s %12s %10s", 
				"id", "file", "type", "cov(%)", "selfP", "instance", "cov_per_instance (%)"));  
				
		list.add("===========================================================" +
		"============================================================================================" +
		"===================");
	}
	
	public static void main(String args[]) {
		String rootDir = "/h/g3/dhjeon/trunk/parasites/pyrprof/test";
		String project = "bandwidth";
		String baseDir = rootDir + "/" + project;
		String sFile = baseDir + "/sregions.txt";
		String dFile = baseDir + "/cpURegion.bin";		
		String outFile = baseDir + "/report.txt";
		
		report(sFile, dFile, outFile);
		
	}
}

package visualizer;

import java.io.*;
import java.util.*;

import pprof.EntryManager;
import pprof.FreqAnalyzer;
import pprof.SRegion;
import pprof.SRegionManager;
import pprof.SelfParallelismAnalyzer;
import pprof.URegion;
import pprof.URegionManager;
public class DotGenerator {
	EntryManager manager;
	SelfParallelismAnalyzer analyzer;
	FreqAnalyzer freq;
	DotGenerator(EntryManager manager) {
		this.manager = manager;
		this.analyzer = new SelfParallelismAnalyzer(manager);
		this.freq = new FreqAnalyzer(manager);
	}
	
	String getNodeString(URegion entry) {
		long parallelism = entry.work / entry.cp;
		/*
		String ret = String.format("\"%d: %s [%d-%d]\"", 
				entry.id, entry.sregion.func,  
				entry.sregion.startLine, entry.sregion.endLine);*/
				
		StringBuffer cBuffer = new StringBuffer("{");		
		int index = 0;
		for (URegion child : entry.children.keySet()) {
			long cnt = entry.children.get(child);
			String part = String.format("<c%d> %d", index, cnt);			
			cBuffer.append(part);
			if (index < entry.children.keySet().size()-1)
				cBuffer.append("|");
			index++;
		}
		cBuffer.append("}");
		
		String children = cBuffer.toString();
		
		long cnt = freq.getFreq(entry);
		long totalWork = cnt * entry.work;
		double percentWork = (double)totalWork / manager.getRoot().work * 100.0; 
		double rp = analyzer.getRP(entry);
		double width = (Math.log(percentWork) + 1) * 1.0;
		double height = (Math.log(rp) + 1) * 1.0;
		
		
		//String ret = String.format("node%d[label = \"{%s [%d-%d] \\n %.2f \\n%d [%d, %d] \\n%d %.2f | %s}\", " +
		String ret = String.format("node%d[label = \"{%s [%d-%d] \\n rs: %.2f \\n%d [%d, %d] \\ncnt: %d \\n coverage: %.2f%%}\", " +
				" width=%.2f, height=%.2f];", 
				entry.id, entry.sregion.func,  
				entry.sregion.startLine, entry.sregion.endLine,
				analyzer.getRP(entry), parallelism, entry.work, entry.cp, 
				cnt, percentWork, //children,
				width, height);
				
		return ret;
	}
	
	String getDEntryString(URegion entry) {
		StringBuffer buf = new StringBuffer();
		String src = getNodeString(entry);
		
		int i = 0;
		for (URegion child : entry.children.keySet()) {
			String dest = getNodeString(child);
			long cnt = entry.children.get(child);
			//if (child.work > 2000)
			if (freq.getCoveragePercentage(child) > 1.0)
				//buf.append(String.format("\tnode%d:c%d -> node%d \n", entry.id, i, child.id));
				buf.append(String.format("\tnode%d -> node%d \n", entry.id, child.id));
			i++;
		}
		return buf.toString();
	}
	
	void generate(String name) {
		SRegionManager sManager = manager.getSRegionManager();
		Set<SRegion> srcSet = sManager.getSRegionSet();
		try {
			Writer output = new BufferedWriter(new FileWriter(name));
			
			output.write("digraph G {\n");
			output.write("rankdir=LR;\n");
			output.write("node [shape=record, fixedsize=true];\n");
			
			for (SRegion each : srcSet) {
				Set<URegion> set = manager.getDEntrySet(each);
				
				for (URegion entry : set) {
					if (freq.getCoveragePercentage(entry) > 1.0)
						output.write(getNodeString(entry) + "\n");
				}
			}
			
			for (SRegion each : srcSet) {
				Set<URegion> set = manager.getDEntrySet(each);
				
				for (URegion entry : set) {
					
						output.write(getDEntryString(entry));
				}
			}
						
			output.write("}\n");
			
			output.close();
		} catch(Exception e) {
			e.printStackTrace();
		}
	}

	public static void main(String args[]) {
		String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/ft";

		System.out.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";

		long start = System.currentTimeMillis();
		SRegionManager sManager = new SRegionManager(new File(sFile), false);
		URegionManager dManager = new URegionManager(sManager, new File(dFile));
		long end = System.currentTimeMillis();
		
		System.out.println("Ready Time = " + (end - start) + " ms");
		
		DotGenerator generator = new DotGenerator(dManager);
		generator.generate(rawDir + "/graph.dot");		
	}
}

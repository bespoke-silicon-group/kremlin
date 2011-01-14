package visualizer;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.*;
import pprof.*;
import runner.OMPGrepReader;

public class PloticusChartGenerator {
	BufferedWriter output;
	SRegionInfoAnalyzer analyzer;
	OMPGrepReader reader;
	Set<SRegion> parentParallelized;
	
	PloticusChartGenerator(SRegionInfoAnalyzer analyzer, OMPGrepReader reader) {
		this.analyzer = analyzer;
		this.reader = reader;		
		this.parentParallelized = new HashSet<SRegion>();
		
		List<SRegionInfo> workList = new ArrayList<SRegionInfo>();
		Set<SRegionInfo> retired = new HashSet<SRegionInfo>();
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			SRegion region = each.getSRegion();
			if (isParallelized(region)) {
				workList.add(each);				
			}
		}
		
		while (workList.isEmpty() == false) {
			SRegionInfo toRemove = workList.remove(0);
			for (SRegion child : toRemove.getChildren()) {
				SRegionInfo info = analyzer.getSRegionInfo(child);
				if (!retired.contains(info)) {
					workList.add(info);
				}
			}
			this.parentParallelized.add(toRemove.getSRegion());
			retired.add(toRemove);
		}
	}
	
	boolean isPromising(double sp, double coverage) {
		if (sp > 5.0 && coverage > 10.0)
			return true;
		
		if (sp > 10)
			return true;
		
		return false;		
	}
	
	boolean parentParallelized(SRegionInfo info) {
		return parentParallelized.contains(info.getSRegion());
	}
	
	boolean isParallelized(SRegion sregion) {
		return (sregion.isFunction() == false) &&
			reader.contains(sregion.getStartLine());
	}
	
	//0: loop, parallelized
	//1: loop, not parallelized
	//2: loop, parent parallelized
	//3: func, parallelized
	//4: func, not parallelized,
	//5: func, parent parallelized
	
	int getType(SRegionInfo info) {
		double sp = info.getSelfParallelism();
		double coverage = info.getCoverage();
		SRegion sregion = info.getSRegion();
		boolean exploited = isParallelized(sregion);
		//boolean promising = isPromising(sp, coverage);
		boolean isFunc = sregion.isFunction();		
		boolean isParentParallelized = parentParallelized(info);
		
		if (isFunc) {
			if (exploited)
				return 0;
			else if (isParentParallelized)
				return 2;
			else
				return 1;
			
		} else {
			if (exploited)
				return 3;
			else if (isParentParallelized)
				return 5;
			else
				return 4;
		}	
		
		
	}
	
	
	// what to write?
	// 1. file
	// 2. func 
	// 3. start line
	// 4. end line
	// 5. region type
	// 6. total parallelism
	// 7. self parallelism
	// 8. exec coverage
	// 9. manually parallelized?
	// 10. ancestor parallelized?
	// 11. extra code
	
	String getEntryString(SRegionInfo each) {
		double sp = each.getSelfParallelism();
		double coverage = each.getCoverage();
		SRegion sregion = each.getSRegion();
		boolean exploited = (sregion.isFunction() == false) &&
			reader.contains(sregion.getStartLine());
		int num = (exploited == true) ? 1 : 0;
		int isFunc = (sregion.isFunction() == true) ? 1 : 0;
		int type = getType(each);
		String out = String.format("%.2f\t%.2f\t%.2f\t%d\t%d\t%d\n", 
				each.getTotalParallelism(), sp, coverage, num, type, isFunc);
		return out;
	}
	
	
	void writeDatFile(String fileName) {
		try {
			this.output = new BufferedWriter(new FileWriter(fileName));
			
			Set<SRegionInfo> set = analyzer.getSRegionInfoSet();
			for (SRegionInfo each : set) {
				String out = getEntryString(each);
				output.append(out);
			}
			
			this.output.close();
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	void write(String fileName, String datFile) {
		try {
			this.output = new BufferedWriter(new FileWriter(fileName));
			//writePage();
			writeData(datFile);
			writeArea();
			
			writePlot0();		
			writePlot1();
			
			writeLegends();
			this.output.close();
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
		
	}
	
	void writePage() throws Exception {
		output.append("#proc page\n");
		output.append("  #if @DEVICE in gif, png\n");
		output.append("    scale: 1.5\n");
		output.append("  #endif\n");
	}
	
	void writeArea() throws Exception {
		output.append("#proc areadef\n");
		output.append("  rectangle: 2 2 6 6\n");
		output.append("  xautorange: datafield=3\n");
		output.append("  yautorange: datafield=4 hifix=105\n");
		output.append("  frame: width=0.5 color=0.3\n");
		output.append("  xaxis.stubs: inc 20\n");
		output.append("  xaxis.stubrange: 0 100\n");
		output.append("  yaxis.stubs: inc\n");
		output.append("  xaxis.label: Self Parallelism\n");
		output.append("  yaxis.label: Work Coverage(%)\n");
		output.append("  xscaletype: log\n");
		output.append("  yscaletype: log\n");
		
	}
	
	void writeData(String datFile) throws Exception {
		output.append("#proc getdata\n");
		output.append("\tpathname: " + datFile + "\n\n");
		/*
		output.append("data:\n");
		Set<SRegionInfo> set = analyzer.getSRegionInfoSet();
		for (SRegionInfo each : set) {
			double sp = each.getSelfParallelism();
			double coverage = each.getCoverage();
			SRegion sregion = each.getSRegion();
			boolean exploited = (sregion.isFunction() == false) &&
				reader.contains(sregion.getStartLine());
			int num = (exploited == true) ? 1 : 0;
			int type = getType(each);
			String out = String.format("%.2f\t%.2f\t%.2f\t%d\t%d\n", 
					each.getTotalParallelism(), sp, coverage, num, type);	
			
			output.append(out);
		}*/
		
	}
	
	void writePlot0() throws Exception {
		output.append("#proc scatterplot\n");
		output.append("  xfield: 3\n");
		output.append("  yfield: 4\n");
		output.append("  symbol: shape=nicecircle fillcolor=black radius=0.05\n");
		//output.append("  legendlable: Alpha type N=@@NVALUES\n");
		output.append("  select: @@5 = 0\n");
	}
	
	void writePlot1() throws Exception {
		output.append("#proc scatterplot\n");
		output.append("  xfield: 3\n");
		output.append("  yfield: 4\n");
		output.append("  symbol: shape=nicecircle fillcolor=yellow radius=0.05\n");
		//output.append("  legendlable: Beta type N=@@NVALUES\n");
		output.append("  select: @@5 = 1\n");
	}
	

	
	void writeLegends() throws Exception {
		output.append("#proc legend\n");
		output.append("  location: min+0.3 max-0.1\n");
	}
}

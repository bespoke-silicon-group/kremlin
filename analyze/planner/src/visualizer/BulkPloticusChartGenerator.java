package visualizer;

import java.io.*;
import java.util.Set;

import pprof.SRegion;
import pprof.SRegionInfo;
import pprof.SRegionInfoAnalyzer;

public class BulkPloticusChartGenerator {
	String inDir;
	BufferedWriter output;
	int startX = -1;	
	int endX = -1;
	boolean onlyFunc = false;
	boolean onlyLoop = false;
	
	public BulkPloticusChartGenerator(String inDir) {
		this.inDir = inDir;		
	}
	
	void setOnlyFunc() {
		onlyFunc = true;
		onlyLoop = false;
	}
	
	void setOnlyLoop() {
		onlyFunc = false;
		onlyLoop = true;
	}
	
	void setBoth() {
		onlyFunc = false;
		onlyLoop = false;
	}
	
	void writeData(String fileName) {
		try {
			this.startX = startX;
			this.output = new BufferedWriter(new FileWriter(fileName));
			//writePage();
			writeDataFile(output);
			this.output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	void writeTotalParallelism(String fileName, int startX) {
		try {
			this.startX = startX;
			this.output = new BufferedWriter(new FileWriter(fileName));
			//writePage();
			writeData(output);
			writeArea("Total Parallelism", true);
			//writeNotParallelizedPlot1();
			//writeNotParallelizedPlot2();
			writeTotal0();
			writeTotal1();
			writeTotal2();						
			
			//writeParallelizedPlot();
			writeLegends();
			this.output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
		
	}
	
	
	void write(String fileName, int startX) {
		try {
			this.startX = startX;
			this.output = new BufferedWriter(new FileWriter(fileName));
			//writePage();
			writeData(output);
			writeArea("Self Parallelism", false);
			
			writePlot0();
			writePlot1();
			writePlot2();			
								
			//writeParallelizedPlot();
			writeLegends();
			this.output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
		
	}
	
	void writeArea(String xAxis, boolean isTotal) throws Exception {
		output.append("#proc areadef\n");
		output.append("  rectangle: 2 2 6 6\n");
		
		
		if (this.onlyFunc && endX == -1 && isTotal == false)
			endX = 10;
		
		String axis = "";
		if (startX == -1 && endX != -1) {
			axis = String.format("hifix=%d", endX);
		}
		if (startX != -1 && endX == -1) {
			axis = String.format("lowfix=%d", startX);
		}
		if (startX != -1 && endX != -1) {
			axis = String.format("lowfix=%d hifix=%d", startX, endX);
		}
		
		
		
		
		output.append("  xautorange: datafield=1 " + axis + "\n");
		output.append("  yautorange: datafield=2 hifix=105\n");
		output.append("  frame: width=0.5 color=0.3\n");
		output.append("  xaxis.stubs: inc\n");
		output.append("  xaxis.stubcull: 0.2\n");
		//output.append("  xaxis.stubrange: 0 100\n");
		output.append("  yaxis.stubs: inc\n");
		output.append("  xaxis.label: " + xAxis + "\n");
		output.append("  yaxis.label: Work Coverage(%)\n");
		output.append("  xscaletype: log\n");
		output.append("  yscaletype: log\n");
		
		startX = -1;
		endX = -1;
		
	}
	
	void writeDataFile(BufferedWriter output) throws Exception {
		//output.append("#proc getdata\n");
		//output.append("data:\n");
				
		File in = new File(inDir);
		assert(in.isDirectory());
		File[] files = in.listFiles();
		for (File each : files) {
			System.out.println("Processing " + each.getAbsolutePath());
			if (each.getAbsolutePath().endsWith(".dat")) {
				BufferedReader reader = new BufferedReader(new FileReader(each));
				while(true) {
					String line = reader.readLine();
					if (line == null)
						break;
					if (line.length() > 1) {
						output.append(line + "\n");
						System.out.println("\t" + line);
					}
					
				}
				reader.close();
			}
		}
		//output.append("\n#set promising = (@@1 >= 10) or (@@2 >= 10)\n");
	}
	
	void writeData(BufferedWriter output) throws Exception {
		output.append("#proc getdata\n");
		output.append("data:\n");
				
		File in = new File(inDir);
		assert(in.isDirectory());
		File[] files = in.listFiles();
		for (File each : files) {
			System.out.println("Processing " + each.getAbsolutePath());
			if (each.getAbsolutePath().endsWith(".dat")) {
				BufferedReader reader = new BufferedReader(new FileReader(each));
				while(true) {
					String line = reader.readLine();
					if (line == null)
						break;
					if (line.length() > 1) {
						output.append("\t" + line + "\n");
						System.out.println("\t" + line);
					}
					
				}
				reader.close();
			}
		}
		//output.append("\n#set promising = (@@1 >= 10) or (@@2 >= 10)\n");
	}
	
	void writePlot0() throws Exception {
		output.append("#proc scatterplot\n");
		output.append("  xfield: 2\n");
		output.append("  yfield: 3\n");
		output.append("  cluster: yes\n");
		output.append("  symbol: shape=nicecircle fillcolor=yellow radius=0.05\n");
		//output.append("  legendlable: Alpha type N=@@NVALUES\n");
		//output.append("  select: @@5 in 1,4\n");
		
		if (this.onlyFunc == false && this.onlyLoop == false)
			output.append("  select: @@5 in 1,4\n");
		else if (this.onlyFunc == true)
			output.append("  select: @@5 in 1\n");
		else 
			output.append("  select: @@5 in 4\n");
		
		output.append("  legendlabel: not parallelized\n");
	}
	
	void writePlot1() throws Exception {
		output.append("#proc scatterplot\n");
		output.append("  xfield: 2\n");
		output.append("  yfield: 3\n");
		output.append("  cluster: yes\n");
		output.append("  symbol: shape=nicecircle fillcolor=green radius=0.05\n");
		//output.append("  legendlable: Beta type N=@@NVALUES\n");
		//output.append("  select: @@5 in 0,3\n");
		if (this.onlyFunc == false && this.onlyLoop == false)
			output.append("  select: @@5 in 0,3\n");
		else if (this.onlyFunc == true)
			output.append("  select: @@5 in 0\n");
		else 
			output.append("  select: @@5 in 3\n");
		
		output.append("  legendlabel: parallelized\n");
	}
	
	void writePlot2() throws Exception {
		output.append("#proc scatterplot\n");
		output.append("  xfield: 2\n");
		output.append("  yfield: 3\n");
		output.append("  cluster: yes\n");
		output.append("  symbol: shape=nicecircle fillcolor=blue radius=0.05\n");
		//output.append("  legendlable: Beta type N=@@NVALUES\n");
		//output.append("  select: @@5 in 2,5\n");
		if (this.onlyFunc == false && this.onlyLoop == false)
			output.append("  select: @@5 in 2,5\n");
		else if (this.onlyFunc == true)
			output.append("  select: @@5 in 2\n");
		else 
			output.append("  select: @@5 in 5\n");
		output.append("  legendlabel: parent parallelized\n");
	}
	

	
	void writeTotal0() throws Exception {
		output.append("#proc scatterplot\n");
		output.append("  xfield: 1\n");
		output.append("  yfield: 3\n");
		output.append("  cluster: yes\n");
		output.append("  symbol: shape=nicecircle fillcolor=yellow radius=0.05\n");
		//output.append("  legendlable: Beta type N=@@NVALUES\n");
		if (this.onlyFunc == false && this.onlyLoop == false)
			output.append("  select: @@5 in 1,4\n");
		else if (this.onlyFunc == true)
			output.append("  select: @@5 in 1\n");
		else 
			output.append("  select: @@5 in 4\n");
		
		output.append("  legendlabel: not parallelized\n");
	}
	
	void writeTotal1() throws Exception {
		output.append("#proc scatterplot\n");
		output.append("  xfield: 1\n");
		output.append("  yfield: 3\n");
		output.append("  cluster: yes\n");
		output.append("  symbol: shape=nicecircle fillcolor=green radius=0.05\n");
		//output.append("  legendlable: Beta type N=@@NVALUES\n");
		//output.append("  select: @@5 in 0,3\n");
		
		if (this.onlyFunc == false && this.onlyLoop == false)
			output.append("  select: @@5 in 0,3\n");
		else if (this.onlyFunc == true)
			output.append("  select: @@5 in 0\n");
		else 
			output.append("  select: @@5 in 3\n");
					
		output.append("  legendlabel: parallelized\n");
	}
	
	void writeTotal2() throws Exception {
		output.append("#proc scatterplot\n");
		output.append("  xfield: 1\n");
		output.append("  yfield: 3\n");
		output.append("  cluster: yes\n");
		output.append("  symbol: shape=nicecircle fillcolor=blue radius=0.05\n");
		//output.append("  legendlable: Beta type N=@@NVALUES\n");
		//output.append("  select: @@5 in 2,5\n");
		
		if (this.onlyFunc == false && this.onlyLoop == false)
			output.append("  select: @@5 in 2,5\n");
		else if (this.onlyFunc == true)
			output.append("  select: @@5 in 2\n");
		else 
			output.append("  select: @@5 in 5\n");
		output.append("  legendlabel: parent parallelized\n");
	}
	
	
	void writeLegends() throws Exception {
		output.append("#proc legend\n");
		output.append("  location: min+3.0 max-0.1\n");
	}
	
	public static void main(String args[]) {
		String root = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/chartGen";
		BulkPloticusChartGenerator generator = new BulkPloticusChartGenerator(root + "/data");
		generator.writeData(root + "/data.dat");
		
		generator.setBoth();
		generator.write(root + "/self_both.pl", 0);
		generator.setOnlyFunc();
		generator.write(root + "/self_func.pl", 0);
		generator.setOnlyLoop();
		generator.write(root + "/self_loop.pl", 0);
		
		
		//generator.endX = 40;
		//generator.write(root + "/chart_0_40.pl", 0);
		//generator.endX = -1;
		//generator.write(root + "/chart_40.pl", 40);
		//generator.write(root + "/chart_100.pl", 100);
		generator.setBoth();
		generator.writeTotalParallelism(root + "/total_both.pl", 0);
		generator.setOnlyFunc();
		generator.writeTotalParallelism(root + "/total_func.pl", 0);
		generator.setOnlyLoop();
		generator.writeTotalParallelism(root + "/total_loop.pl", 0);
	}
}

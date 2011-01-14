package visualizer;

import java.io.*;

public class DataFileMerger {
	String inDir;
	DataFileMerger(String inDir) {
		this.inDir = inDir;
	}
	
	void writeDataFile(String file) {
		//output.append("#proc getdata\n");
		//output.append("data:\n");
		try {
			BufferedWriter output = new BufferedWriter(new FileWriter(file));		
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
			output.close();
		} catch(Exception e) {
			e.printStackTrace();
		}
		//output.append("\n#set promising = (@@1 >= 10) or (@@2 >= 10)\n");
	}
	
	public static void merge(String inDir, String outFile) {
		DataFileMerger merger = new DataFileMerger(inDir);
		merger.writeDataFile(outFile);
	}
	
	public static void main(String args[]) {
		String root = null;
		if (args.length < 1) {
			root = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/chartGen";
		} else {
			root = args[0];
		}
		
		DataFileMerger.merge(root + "/data", root + "/merged.dat");
	}
}

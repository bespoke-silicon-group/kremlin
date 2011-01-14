package omp;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.*;

import pprof.Config;
import pprof.Util;

public class ReplayRunner {
	public static void main(String[] argv) {
		Config.pprofDir = argv[0];
		String bench = argv[1];
		String input = argv[2];
		String pec = argv[3];
		int core = Integer.parseInt(argv[4]);
		
		System.out.println("Hello ReplayRunner " + bench + " " + input + " " + pec);
		ReplayRunner runner = new ReplayRunner(bench, input, pec, core);		
		List<FeedbackOmp> res = runner.run();
		UtilOmp.report(res, Config.pprofDir + "/" + runner.base, "report.txt");
		//runner.report(res, Config.pprofDir + "/" + runner.base);
	}
	
	List<Integer> pec;
	List<Integer> serialized;
	String bench;
	String base;
	String inputSize;
	String pecFile;
	int numCore;
	
	ReplayRunner(String bench, String input, String pec, int numCore) {
		this.bench = bench;
		this.inputSize = input;
		this.pecFile = pec;
		this.pec = Util.readManualFile(this.pecFile);
		this.base = "replay." + bench + "." + inputSize + "." + numCore;
		this.numCore = numCore;
		Util.prepareBaseDir(base);		
	}
	
	List<Integer> createParallelizedList(int n) {
		List<Integer> ret = new ArrayList<Integer>();
		for (int i=0; i<n; i++) {
			ret.add(pec.get(i)-1);
		}
		return ret;
	}
	
	String getDirName(int n) {
		if (n == 0) 
			return "0_serial";
		else
			return String.format("%d_%d", n, pec.get(n-1) - 1);
	}
	
	List<FeedbackOmp> run() {
		List<FeedbackOmp> list = new ArrayList<FeedbackOmp>();		
		
		for (int i=0; i<=pec.size(); i++) {
			List<Integer> parallelList = createParallelizedList(i);
			String name = getDirName(i);
			FeedbackOmp feedback = runParallelized(name, parallelList, numCore);
			list.add(i, feedback);
		}			
		return list;
	}	
	
	FeedbackOmp runParallelized(String runName, List<Integer> parallelized, int core) {
		String parallelFile = Config.pprofDir + "/" + base + "/parallel/" + runName + ".parallel";		
		Util.createParallelFile(parallelized, parallelFile);
		
		UtilOmp.makeRun(runName, bench, base, inputSize, core, parallelFile, false);
		FeedbackOmp ret = new FeedbackOmp(Config.pprofDir + "/" + base + "/" + runName);
		System.out.println("\tMOPS = " + ret.getTime());		 
		return ret;		
	}
	
	void report(List<FeedbackOmp> list, String dir) {
		String fileName = dir + "/" + "report.txt";		
		
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(fileName));
			
			//String last = "serial";
			for (int i=0; i<list.size(); i++) {
				FeedbackOmp feedback = list.get(i);
				int index = feedback.getDir().lastIndexOf("_");
				String last = feedback.getDir().substring(index+1);				
				String str = String.format("%d\t%10s\t%f\n", i, last, feedback.getTime());
				System.out.printf(str);
				output.write(str);
				
			}		
			output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}		
	}
	
}

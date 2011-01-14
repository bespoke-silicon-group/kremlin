package omp;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.*;

import pprof.Config;
import pprof.Util;

public class BruteForceRunner {
	public static void main(String argv[]) {
		runBruteForce(argv[0], argv[1], argv[2], argv[3], Integer.parseInt(argv[4]));
	}
	
	public static void runBruteForce(String pprofDir, String bench, String input, String manualFile, int numCore) {
		Config.pprofDir = pprofDir;
		BruteForceRunner runner = new BruteForceRunner(bench, input, manualFile, numCore);
		List<FeedbackOmp> res = runner.run();
		runner.report(res, pprofDir + "/" + runner.base);
		//Util.report(res, pprofDir + "/" + runner.base, "report.txt");
	}
	
	BruteForceRunner(String bench, String inputSize, String manual, int numCore) {		 
		this.manual = Util.readManualFile(manual);		
		this.serialized = new ArrayList<Integer>();
		this.bench = bench;
		this.inputSize = inputSize;
		this.base = "brute." + bench + "." + inputSize + "." + numCore;
		this.numCore = numCore;
		Util.prepareBaseDir(base);		
	}
	
	List<Integer> manual;
	List<Integer> serialized;
	String bench;
	String base;
	String inputSize;
	int numCore;
	
	List<FeedbackOmp> run() {
		List<FeedbackOmp> list = new ArrayList<FeedbackOmp>();
		
		for (int i=0; i<=manual.size(); i++) {			
			if (i == 0) {
				// run fully parallel version
				FeedbackOmp feedback = runSerialized("0_parallel", new ArrayList<Integer>(), numCore);
				list.add(0, feedback);
				continue;
			}
			/*
			if (i == manual.size()) {
				ExecFeedback feedback = runSerialized(i + "_serial", new ArrayList<Integer>(manual), 1);
				list.add(0, feedback);
				continue;
			}*/
			
			// generate parallel file after removing "serialized" + 1 region
			List<Integer> temp = new ArrayList<Integer>(manual);
			temp.removeAll(serialized);
			Map<Integer, FeedbackOmp> resMap = new HashMap<Integer, FeedbackOmp>();
			float best = 0;
			int bestRegion = -1;
			
			for (Integer each : temp) {
				List<Integer> target = new ArrayList<Integer>(serialized);
				target.add(each);
				String name = String.format("%d_%d", i, each);
				FeedbackOmp feedback = runSerialized(name, target, numCore);
				
				if (feedback.getTime() < best || bestRegion == -1) {
					bestRegion = each;
					best = feedback.getTime();
				}
				resMap.put(each, feedback);				
			}
			
			FeedbackOmp feedback = resMap.get(bestRegion);
			list.add(0, feedback);
			serialized.add(bestRegion);
			System.out.println("picking " + bestRegion + " perf: " + best);
		}
		return list;
	}
	
	static int cnt = 0;
	FeedbackOmp runSerialized(String runName, List<Integer> serialize, int core) {				
		List<Integer> parallelized = new ArrayList<Integer>(manual);
		parallelized.removeAll(serialize);
		
		String parallelFile = Config.pprofDir + "/" + base + "/parallel/" + runName + ".parallel";		
		Util.createParallelFile(parallelized, parallelFile);
		
		UtilOmp.makeRun(runName, bench, base, inputSize, core, parallelFile, false);
		FeedbackOmp ret = new FeedbackOmp(Config.pprofDir + "/" + base + "/" + runName);
		System.out.println("\tTime = " + ret.getTime());		
		
		return ret;		
	}
	
	void report(List<FeedbackOmp> list, String dir) {
		String fileName = dir + "/" + "report.txt";		
		
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(fileName));
			
			String last = "serial";
			for (int i=0; i<list.size(); i++) {
				FeedbackOmp feedback = list.get(i);
				String str = String.format("%d\t%10s\t%f\n", i, last, feedback.getTime());
				System.out.printf(str);
				output.write(str);
				int index = feedback.getDir().lastIndexOf("_");
				last = feedback.getDir().substring(index+1);
			}		
			output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}		
	}
	
}

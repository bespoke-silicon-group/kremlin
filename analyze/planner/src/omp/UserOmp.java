package omp;


import java.util.*;
import java.io.*;
import java.lang.*;

import pprof.*;
public class UserOmp {	

	public UserOmp(String file, String bench, String inputSize, String baseDir, int numCore) {
		this.targetSet = Util.readManualFile(file);
		this.parallelized = new HashSet<SRegion>();
		this.bench = bench;
		this.inputSize = inputSize;
		this.base = baseDir;
		this.numCore = numCore;
				
		Util.prepareBaseDir(base);		
		calibrate();
	}
	
	List<Integer> targetSet;
	Set<SRegion> parallelized;
	//Feedback serialFeedback;
	String serialDir;
	String bench;
	String base;
	String inputSize;
	int numCore;
	
	String getSerialFeedbackDir() {
		return serialDir;
	}
	
	void calibrate() {
		System.out.println("Calibrating Serial Performance");
		this.serialDir = runParallelized(null, numCore);
	}
	
	// run a sequential version and get results
	/*
	 * 
	void calibrate() {
		String name = "calibrate";
		String base = "work";		
		int core = 1;
		String parallelFile = Config.pprofDir + "/ft.parallel";
		
		Util.makeRun(name, bench, base, inputSize, core, parallelFile);
		serialFeedback = new ExecFeedback(Config.pprofDir + "/" + base + "/" + name + "/region.txt");
	}	*/
	
	
	boolean isExploitable(SRegion region) {
		if (region.isFunction() == true)
			return false;
		
		if (!targetSet.contains(region.getStartLine())) {
			return false;
		}
		
		return true;
	}
	
	/**
	 * add a new sregion
	 * ,and parallelelize
	 */
	static int cnt = 0;
	/*
	FeedbackOmp runParallelized(SRegion add, int core) {
		String name = null;
		
		if (add != null) {
			parallelized.add(add);
			name = cnt++ + "_" + add.getStartLine();
			
		} else {
			name = cnt++ + "_serial";
		}		
		 
		String parallelFile = Config.pprofDir + "/" + base + "/parallel/" + name + ".parallel";
		
		Util.createParallelFile(parallelized, parallelFile);
		Util.makeRun(name, bench, base, inputSize, core, parallelFile);
		FeedbackOmp ret = new FeedbackOmp(Config.pprofDir + "/" + base + "/" + name);
		System.out.println("\tMOPS = " + ret.getMops());		
		return ret;		
	} */

	String runParallelized(SRegion add, int core) {
		String name = null;

		if (add != null) {
			parallelized.add(add);
			name = cnt++ + "_" + add.getStartLine();

		} else {
			name = cnt++ + "_serial";
		}

		String parallelFile = Config.pprofDir + "/" + base + "/parallel/" + name + ".parallel";

		Util.createParallelFile(parallelized, parallelFile);
		UtilOmp.makeRun(name, bench, base, inputSize, core, parallelFile, true);
		String ret = Config.pprofDir + "/" + base + "/" + name;
		return ret;		
	}
	
	
	
}

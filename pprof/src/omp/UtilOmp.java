package omp;
import java.util.*;
import java.io.*;

import pprof.*;

public class UtilOmp {
	
	public static void report(List<FeedbackOmp> list, String dir, String name) {
		String fileName = dir + "/" + name;		
		
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(fileName));
			
			//String last = "serial";
			for (int i=0; i<list.size(); i++) {
				FeedbackOmp feedback = list.get(i);
				int index = feedback.getDir().lastIndexOf("_");
				String last = feedback.getDir().substring(index+1);				
				String str = String.format("%d\t%10s\t%.3f\n", i, last, feedback.getTime());
				System.out.printf(str);
				output.write(str);
				
			}		
			output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}		
	}
	
	public static FeedbackOmp readFeedbackPerf(String dir) {
		return new FeedbackOmp(dir);
	}	
	
	public static Feedback readFeedback(RegionManager manager, String dir) {
		return new Feedback(manager, dir + "/feedback.txt");
	}
	
	public static SRegionStateMap readSRegionMap(RegionManager manager, String dir) {
		return new SRegionStateMap(manager, dir + "/region.desc");
	}
	
	public static void makeRun(String name, String bench, String destBase, String input, 
			int core, String parallel, boolean doInstrument) {
		String exec = Config.pprofDir + "/scripts/setupExp.py";
		//String stdout = destBase + "/" + "std.out";
		//String stderr = destBase + "/" + "std.err";
		String line = String.format("%s %s %s %s %s %s %d %s", 
				exec, name, Config.pprofDir, bench, destBase, input, core, parallel);		
		
		String destDir = Config.pprofDir + "/" + destBase + "/" + name;
		String resultFile = destDir + "/perf.txt";
						
		if (new File(resultFile).exists()) {
			System.out.println("Skipping..execution results available..");
			return;
		}		
		Util.runCmd(line);
		
		// add instrumentation
		if (doInstrument) {
			String rawDir = Config.pprofDir + "/rawData/" + bench + "." + input;
			String[] args = {rawDir, destDir};
			//InstrumentorOmp.main(args);
		}
					
		// create script file
		String scriptGen = Config.pprofDir + "/scripts/createScriptInteractive.py";
		String qsubFile = String.format("%s/qsub.script", destDir);
		String scriptName = destBase + "_" + name;
		String script = String.format("%s %s %s %s %s %s %s %s", 
				scriptGen, scriptName, Config.pprofDir, bench, input, core, destDir, qsubFile);
		Util.runCmd(script);
		
		// run script file		
		String submit = String.format("qsub %s",  qsubFile);
		if (!new File(qsubFile).exists()) {
			System.out.println("Error: qsub file " + qsubFile + " does not exist");
			assert(false);
			return;
		}
		Util.runCmd(submit);
		
		
		int cnt = 0;
		int timeover = 1000;
		System.out.println("Waiting for " + resultFile + " is ready..");
		while(true) {
			cnt++;
			if (cnt > timeover) {
				System.out.println("Check dir " + destDir + 
						" App does not complete in " + timeover + " secs");
				assert(false);
			}
			if (new File(resultFile).exists()) {
				break;
			}
			try {
				Thread.sleep(1000);
			} catch(Exception e) {
				e.printStackTrace();
				assert(false);
			}
			System.out.print(".");
		}
		System.out.print("\n");
		
	}	
	
}

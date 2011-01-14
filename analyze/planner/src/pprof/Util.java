package pprof;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.util.*;

import omp.FeedbackOmp;

public class Util {
	public static void createParallelFile(Set<SRegion> parallelized, String outFile) {
		System.out.println("Creating " + outFile);
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(outFile));
			
			for (SRegion region : parallelized) {
				String line = String.format("%d\n", region.startLine - 1);
				output.write(line);
			}
			output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	public static boolean isReduction(SRegion region) {
		int startLine = region.getStartLine();
		
		if (region.getType() != RegionType.LOOP)
			return false;
		
		if (startLine == 593)
			return true;
		/*
		if (startLine == 444)
			return true;
		
		if (startLine == 419)
			return true;
		if (startLine == 405)
			return true;
		if (startLine == 435)
			return true;
		if (startLine == 413)
			return true;
		if (startLine == 468)
			return true; */
		
		return false;
	}
	
	public static void createParallelFile(List<Integer> parallelized, String outFile) {
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(outFile));
			
			for (int num : parallelized) {
				String line = String.format("%d\n", num - 1);
				output.write(line);
			}
			output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	public static List<Integer> readManualFile(String file) {
		List<Integer> ret = new ArrayList<Integer>();
		
		try {
			BufferedReader input =  new BufferedReader(new FileReader(file));
			
			while(true) {
				String line = input.readLine();
				if (line == null)
					break;
				ret.add(Integer.parseInt(line)+1); 
			}
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
		return ret;
	}
	
	public static void runCmd(String cmd) {
		//System.out.println("Exec " + cmd);
		try {
			Process child = Runtime.getRuntime().exec(cmd);
			child.waitFor();
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}	
	
	
	public static void prepareBaseDir(String base) {
		String exec = Config.pprofDir + "/scripts/createWorkDir.py";
		String srcDir = Config.pprofDir + "/src";
		String destDir = Config.pprofDir + "/" + base;
		String cmd = String.format("%s %s %s", exec, srcDir, destDir);
		runCmd(cmd);
	}
	
		
	
	
}

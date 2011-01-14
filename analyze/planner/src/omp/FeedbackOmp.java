package omp;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.*;
import pprof.*;

public class FeedbackOmp {
	Map<Integer, List<Long>> map;
	float time;
	String base;
		
	FeedbackOmp(String dir) {
		//super(manager, dir + "/region.txt");
		
		base = dir;		
		String perfFile = dir + "/perf.txt";		
		loadPerfFile(perfFile);	
		
	}
	
	void loadPerfFile(String file) {
		try {
			BufferedReader input =  new BufferedReader(new FileReader(file));
			
			while(true) {
				String line = input.readLine();
				if (line == null)
					break;
				
				List<String> list = new ArrayList<String>(); 
				StringTokenizer st = new StringTokenizer(line); 
				while(st.hasMoreElements()) {
					list.add(st.nextToken());
				}
				if (list.size() < 5)
					break;
				this.time = Float.parseFloat(list.get(4));
			}
			
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
		
	
	
	Set<Integer> getStartLineSet() {
		Set<Integer> ret = new HashSet<Integer>(map.keySet());		
		return ret;
	}
	
	float getTime() {
		return time;
	}	
	
	String getDir() {
		return base;
	}
}

package runner;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.*;
import pprof.SRegion;

public class OMPGrepReader {
	File file;
	Set<Integer> lines;
	
	public OMPGrepReader() {
		this.lines = new HashSet<Integer>();
	}
	
	public OMPGrepReader(String file) {
		this.file = new File(file);
		this.lines = new HashSet<Integer>();
		parseFile(this.file);
	}
	
	void parseFile(File file) {
		try {
			BufferedReader input =  new BufferedReader(new FileReader(file));
			
			while(true) {
				String line = input.readLine();
				if (line == null)
					break;
				processLine(line);
				//System.out.println(line);
			}
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	void processLine(String line) {		
		String terms[] = line.split(":");
		String func = terms[0];
		String lineNum = terms[1];
		String pragma = terms[2];
		System.out.printf("%s %s %s\n", func, lineNum, pragma);
		if (pragma.contains("omp for") || pragma.contains("omp parallel for")) {
			lines.add(Integer.parseInt(lineNum) + 1);
		}		
	}
	
	void dump() {
		for (int each : lines) {
			System.out.println(each);
		}
	}
	
	public boolean contains(int line) {
		return lines.contains(line);
	}
	
	public int size() {
		return lines.size();
	}
	
	public static void main(String args[]) {
		OMPGrepReader reader = new OMPGrepReader("/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb/cg/omp.txt");
		reader.dump();
	}
}

package planner;

import java.io.*;
import java.util.*;

public class Util {
	
	public static List<String> getStringList(String file) {
		List<String> ret = new ArrayList<String>();
		System.out.println("Reading " + file);
		try {
			BufferedReader reader = new BufferedReader(new FileReader(file));
			while (true) {
				String line = reader.readLine();
				if (line == null)
					break;
				if (line.length() < 1)
					break;
				else
					ret.add(line);				
			}
			
		} catch(Exception e) {
			e.printStackTrace();		
			assert(false);
		}
		return ret;
	}
	
	public static List<Long> readFile(String file) {
		List<Long> ret = new ArrayList<Long>();
		
		try {
			BufferedReader reader = new BufferedReader(new FileReader(file));
			while (true) {
				String line = reader.readLine();
				if (line == null)
					break;
				if (line.length() < 1)
					break;
				ret.add(Long.parseLong(line));
			}
			
		} catch(Exception e) {
			e.printStackTrace();		
		}
		
		return ret;
	}
	
	public static void writeFile(List<String> list, String file) {		
		
		try {
			BufferedWriter writer = new BufferedWriter(new FileWriter(file));
			for (String line : list) {
				writer.write(line + "\n");
			}
			writer.close();
			
		} catch(Exception e) {
			e.printStackTrace();		
		}		
	}
	
	
}

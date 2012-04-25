package textAnalyzer;

import java.io.BufferedReader;
import java.io.FileReader;
import pprof.*;
import java.util.*;

public class DataEntryManager {
	List<DataEntry> list;
	public DataEntryManager() {
		list = new ArrayList<DataEntry>();
	}
	
	public List<DataEntry> getList() {
		return new ArrayList<DataEntry>(list);
	}
	
	public List<DataEntry> getList(RegionType type) {
		List<DataEntry> ret = new ArrayList<DataEntry>();
		for (DataEntry each : list) {
			if (each.type == type) {
				ret.add(each);
			}				
		}
		return ret;
	}
	
	
	public void readFile(String file) {
		try {
			BufferedReader input =  new BufferedReader(new FileReader(file));
			
			while(true) {
				String line = input.readLine();
				if (line == null)
					break;
				DataEntry entry = new DataEntry(line);
				//System.out.println(entry);
				list.add(entry);
			}
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}	
	
}

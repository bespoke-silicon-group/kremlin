package pprof;

import java.io.*;
import java.util.*;

public class SRegionStateMap {	
	List<SRegion> serial;
	List<SRegion> parallel;
	List<SRegion> exclude;
	
	RegionManager manager;
	
	final static int SERIAL = 0;
	final static int PARALLEL = 1;
	final static int EXCLUDE = 2;
	
	public SRegionStateMap(RegionManager manager) {		
		//this.map = new HashMap<Integer, Integer>();
		this.serial = new ArrayList<SRegion>();
		this.parallel = new ArrayList<SRegion>();
		this.exclude = new ArrayList<SRegion>();		
		this.manager = manager;		
	}
	
	public SRegionStateMap(RegionManager manager, String file) {		
		//this.map = new HashMap<Integer, Integer>();
		this.serial = new ArrayList<SRegion>();
		this.parallel = new ArrayList<SRegion>();
		this.exclude = new ArrayList<SRegion>();		
		this.manager = manager;
		loadFile(file);
	}
	
	public void dump() {
		for (SRegion region : serial) {
			String line = String.format("%d\t%s", region.getID(), "serial");
			System.out.println(line);
		}
		
		for (SRegion region : parallel) {
			String line = String.format("%d\t%s", region.getID(), "parallel");
			System.out.println(line);
		}
		
		for (SRegion region : exclude) {
			String line = String.format("%d\t%s", region.getID(), "exclude");
			System.out.println(line);
		} 
	}
	
	public void dumpFile(String file) {
		//System.out.println("Dumping map " + file);
		BufferedWriter output = null;
		try {
			output =  new BufferedWriter(new FileWriter(file));
			for (SRegion region : serial) {
				String line = String.format("%d\t%s\n", region.getID(), "serial");
				output.write(line);
				//System.out.println(line);
			}
			
			for (SRegion region : parallel) {
				String line = String.format("%d\t%s\n", region.getID(), "parallel");
				output.write(line);
				//System.out.println(line);
			}
			
			for (SRegion region : exclude) {
				String line = String.format("%d\t%s\n", region.getID(), "exclude");
				output.write(line);
				//System.out.println(line);
			}
			/*
			for (SRegion region : manager.getSRegionList()) {
				String type = "serial";
				if (this.parallel.contains(region))
					type = "parallel";
				else if (this.exclude.contains(region))
					type = "exclude";
				
				String line = String.format("%d\t%s\n", region.getID(), type);
				output.write(line);
				//System.out.println(line);
			}*/
			output.close();
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
		
		
	}
	
	public void loadFile(String file) {
		System.out.println("Reading " + file);
		try {
			BufferedReader input =  new BufferedReader(new FileReader(file));
			
			while(true) {
				String line = input.readLine();
				if (line == null)
					break;
				String[] splitted = line.split("\t");
				int id = Integer.parseInt(splitted[0]);
				String state = splitted[1];
			
				if (state.equals("serial"))
					serial.add(manager.getSRegion(id));
				else if (state.equals("parallel"))
					parallel.add(manager.getSRegion(id));					
				else if (state.equals("exclude"))
					exclude.add(manager.getSRegion(id));					
				else {
					System.err.println("Unexpected State: " + line);
					assert(false);
				}
			}
			
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	public void setParallelized(SRegion target) {
		assert(serial.contains(target));
		serial.remove(target);
		parallel.add(target);
	}
	
	public void setExcluded(SRegion target) {		
		assert(exclude.contains(target) == false);
		assert(parallel.contains(target) == false);
		serial.remove(target);
		exclude.add(target);
	}
	
	List<SRegion> getExcludeList() {
		return new ArrayList<SRegion>(exclude);
	}
	
	List<SRegion> getParallelList() {
		return new ArrayList<SRegion>(parallel);		
	}
	
	List<SRegion> getSerialList() {
		return new ArrayList<SRegion>(serial);		
	}
	
}

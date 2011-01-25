package pprof;
import java.util.*;

import pyrplan.Util;

public class SRegionReader {
	public static SRegion createSRegion(String line, boolean newFormat) {
		if (newFormat)
			return createSRegionNew(line);
		else
			return createSRegionOld(line);
	}
	
	private static SRegion createSRegionNew(String line) {
		String splitted[] = line.split("\t");
		long id = Long.parseLong(splitted[0].trim());
		String type = splitted[1].trim();
		String module = splitted[2].trim();
		String func = splitted[3].trim();
		String name = "N/A";
		int start = Integer.parseInt(splitted[4].trim());
		int end = Integer.parseInt(splitted[5].trim());
		SRegion ret = new SRegion(id, name, module, func, start, end, getType(type));
		return ret;
	}
	
	private static SRegion createSRegionOld(String line) {
		StringTokenizer tokenizer = new StringTokenizer(line);
		List<String> list = new ArrayList<String>();
				
		while (tokenizer.hasMoreTokens()) {
			list.add(tokenizer.nextToken());
		}
		//System.out.println(list);
		int id = Integer.parseInt(list.get(0));
		String type = list.get(1);
		String module = list.get(2);
		String func = list.get(3);		
		int start = Integer.parseInt(list.get(4));
		int end = Integer.parseInt(list.get(5));
		//String name = labelMap.get(id);
		String name = "N/A";
		if (name == null) {
			//System.out.println("\n[Error] Region id " + id + " does not have its corresponding entry in label-map.txt");			
			//assert(false);
		}
		
		//System.out.printf("%d %s %s %s %d %d\n", id, type, module, func, start, end);
		SRegion ret = new SRegion(id, name, module, func, start, end, getType(type));
		return ret;
	}
	
	static RegionType getType(String src) {
		if (src.equals("func"))
			return RegionType.FUNC;
		else if (src.equals("loop"))
			return RegionType.LOOP;
		else if (src.equals("loop-body"))
			return RegionType.BODY;
		else if (src.equals("loop_body"))
			return RegionType.BODY;
		
		else {
			assert(false);
			return null;
		}			
	}	
}

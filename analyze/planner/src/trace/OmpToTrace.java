package trace;

import pyrplan.Util;
import java.util.*;

public class OmpToTrace {
	public static void main(String args[]) {
		String base = "/h/g3/dhjeon/research/spaa2011/meta";
		String name = "sp";
		String file = String.format("%s/%s.manual", base, name);
		String outFileSerial = String.format("%s/%s.serial", base, name);
		String outFileParallel = String.format("%s/%s.parallel", base, name);
		String outFileMap = String.format("%s/%s.map", base, name);
		List<Long> list = Util.readFile(file);
		List<String> buffer = new ArrayList<String>();
		List<String> mapBuffer = new ArrayList<String>();
		
		for (long each : list) {
			String updated = String.format("%s:%d", name, each+1);
			buffer.add(updated);
			String mapString = String.format("%s - %s", updated, updated);
			mapBuffer.add(mapString);
		}
		
		Util.writeFile(buffer, outFileSerial);
		Util.writeFile(buffer, outFileParallel);
		Util.writeFile(mapBuffer, outFileMap);		
	}
}

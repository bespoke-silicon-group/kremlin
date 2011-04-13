package pprof;

import java.io.DataInputStream;
import java.io.FileInputStream;
import java.util.*;

public class TraceReader {
	List<TraceEntry> list;
	public TraceReader(String file) {
		list = new ArrayList<TraceEntry>();
		
		try {
			DataInputStream input =  new DataInputStream(new FileInputStream(file));			
		
			while(true) {
				Set<Long> childrenSet = new HashSet<Long>();
				//Map<Long, Long> childrenMap = new HashMap<Long, Long>();
				long uid = Long.reverseBytes(input.readLong());
				long sid = Long.reverseBytes(input.readLong());
				long callSiteValue = Long.reverseBytes(input.readLong());
				long cnt = Long.reverseBytes(input.readLong());
				long work = Long.reverseBytes(input.readLong());				
				long tpWork = Long.reverseBytes(input.readLong());
				long spWork = Long.reverseBytes(input.readLong());
				
				double minSP = (Long.reverseBytes(input.readLong())) / 100.0;
				double maxSP = (Long.reverseBytes(input.readLong())) / 100.0;
				long readCnt = Long.reverseBytes(input.readLong());
				long writeCnt = Long.reverseBytes(input.readLong());
				long loadCnt = Long.reverseBytes(input.readLong());
				long storeCnt = Long.reverseBytes(input.readLong());
				//long loadCnt = 0;
				//long storeCnt = 0;
				
								
				long nChildren = Long.reverseBytes(input.readLong());
				//assert(cp != 0);

				for (int i=0; i<nChildren; i++) {
					long childUid = Long.reverseBytes(input.readLong());
					childrenSet.add(childUid);
				}				
				//System.out.printf("[%d %d]*%d\t %d %d %d %d %d children: %d %s\n",
				//		uid, callSiteValue, cnt, work, tpWork, spWork, readCnt, writeCnt, nChildren, sregion);				
				TraceEntry entry = new TraceEntry(uid, sid, callSiteValue, cnt, work, tpWork, spWork, childrenSet);				
				entry.minSP = minSP;
				entry.maxSP = maxSP;
				entry.readCnt = readCnt;
				entry.writeCnt = writeCnt;
				entry.loadCnt = loadCnt;
				entry.storeCnt = storeCnt;
				
				list.add(entry);
			}			
			
		} catch(Exception e) {
			//System.out.println(e);
			if (e instanceof java.io.EOFException == false) {
				e.printStackTrace();
				assert(false);
			}
		}
	}
	
	List<TraceEntry> getTraceList() {
		return list;
	}
	
	public void dump() {
		for (TraceEntry each : list) {
			System.out.println(each);
		}
	}
}

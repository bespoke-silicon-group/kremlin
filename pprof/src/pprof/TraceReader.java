package pprof;

import java.io.DataInputStream;
import java.io.FileInputStream;
import java.util.*;

public class TraceReader {
	List<TraceEntry> list;
	Map<Long, TraceEntry> map;
	public TraceReader(String file) {
		list = new ArrayList<TraceEntry>();
		map = new HashMap<Long, TraceEntry>();

		try {
			DataInputStream input =  new DataInputStream(new FileInputStream(file));			
		
			while(true) {				
				//Map<Long, Long> childrenMap = new HashMap<Long, Long>();
				long uid = Long.reverseBytes(input.readLong());
				long sid = Long.reverseBytes(input.readLong());
				long callSiteValue = Long.reverseBytes(input.readLong());
				long type = Long.reverseBytes(input.readLong());
				assert(type >=0 && type <= 2);
				
				TraceEntry entry = new TraceEntry(uid, sid, callSiteValue, type);
				map.put(uid, entry);
				long recurse = Long.reverseBytes(input.readLong());
				if (recurse != 0)
					entry.setRecursionTarget(recurse);
				
				long cnt = Long.reverseBytes(input.readLong());
				long pbit = Long.reverseBytes(input.readLong());				
				entry.setNumInstance(cnt).setPBit(pbit != 0);
				
				long nChildren = Long.reverseBytes(input.readLong());
				for (int i=0; i<nChildren; i++) {
					long childUid = Long.reverseBytes(input.readLong());
					entry.addChild(childUid);
					//System.out.printf(" %d ", childUid);
				}				
				
				long nStats = Long.reverseBytes(input.readLong());
				//System.out.printf("\nid: %d sid: %x cid: %x type: %d rtarget: %d instance: %d pbit %d nChildren: %d nStats: %d\n",
				//		uid, sid, callSiteValue, type, recurse, cnt, pbit, nChildren, nStats);
				
				for (int i=0; i<nStats; i++) {
					long nInstance = Long.reverseBytes(input.readLong());
					long work = Long.reverseBytes(input.readLong());				
					long tpWork = Long.reverseBytes(input.readLong());
					long spWork = Long.reverseBytes(input.readLong());				
					double minSP = (Long.reverseBytes(input.readLong())) / 100.0;
					double maxSP = (Long.reverseBytes(input.readLong())) / 100.0;
					long totalIter = Long.reverseBytes(input.readLong());				
					long minIter = Long.reverseBytes(input.readLong());
					long maxIter = Long.reverseBytes(input.readLong());
					CRegionStat toAdd = new CRegionStat(nInstance, work, tpWork, spWork, minSP, maxSP, totalIter, minIter, maxIter);
					entry.addStat(toAdd);
					//System.out.printf("\t%d %d %d %d %.2f %.2f %d %d %d\n", nInstance, work, tpWork, spWork, minSP, maxSP, totalIter, minIter, maxIter);
				}								
								
				
				//System.out.printf("[%d %d %d] instance = %d\n", totalChildCnt, minChildCnt, maxChildCnt, cnt);
				
				//, cnt, work, tpWork, spWork, childrenSet);				
				long totalChildCnt = 0;
				long minChildCnt = 0;
				long maxChildCnt = 0;				
				entry.totalChildCnt = totalChildCnt;
				entry.minChildCnt = minChildCnt;
				entry.maxChildCnt = maxChildCnt;
				
				list.add(entry);
			}			
			
		} catch(Exception e) {			
			if (e instanceof java.io.EOFException == false) {
				e.printStackTrace();
				assert(false);
			}
		}
	}
	
	List<TraceEntry> getTraceList() {
		return list;
	}
	
	TraceEntry getEntry(long id) {
		return map.get(id);
	}
	
	public void dump() {
		for (TraceEntry each : list) {
			System.out.println(each);
		}
	}
}

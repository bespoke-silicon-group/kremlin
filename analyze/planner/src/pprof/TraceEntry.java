package pprof;
import java.util.*;
public class TraceEntry {
	long uid, sid, callSiteValue, cnt, work, tpWork, spWork;
	double minSP, maxSP;
	long readCnt, writeCnt, loadCnt, storeCnt;	
	Set<Long> childrenSet;
	
	public TraceEntry(long uid, long sid, long callSiteValue, long cnt, long work, long tpWork, long spWork, Set<Long> childrenSet) {
		this.uid = uid;
		this.sid = sid;
		this.callSiteValue = callSiteValue;
		this.cnt = cnt;
		this.work = work;
		this.tpWork = tpWork;
		this.spWork = spWork;
		this.childrenSet = new HashSet<Long>(childrenSet);
	}
	
	public String toString() {
		return String.format("<0x%8x 0x%8x 0x%8x> %10d <%10d %10d %10d> <%8.2f %8.2f> <%10d %10d %10d %10d> %s", 
			uid, sid, callSiteValue, cnt, work, tpWork, spWork, minSP, maxSP, readCnt, writeCnt, loadCnt, storeCnt, childrenSet);
	}
}

package pprof;
import java.util.*;
public class TraceEntry {
	long uid, sid, callSiteValue, type, cnt, work, tpWork, spWork;
	double minSP, maxSP;
	long recursionTarget;
	//long readCnt, writeCnt, loadCnt, storeCnt;
	long totalChildCnt, minChildCnt, maxChildCnt;
	boolean pbit;
	Set<Long> childrenSet;
	List<CRegionStat> statList;
	
	public TraceEntry(long uid, long sid, long callSiteValue, long type) {	
		this.uid = uid;
		this.sid = sid;
		this.callSiteValue = callSiteValue;
		//this.cnt = cnt;		
		this.childrenSet = new HashSet<Long>();
		this.statList = new ArrayList<CRegionStat>();
		this.type = type;
		this.recursionTarget = 0;
	}
	//long cnt, long work, long tpWork, long spWork, Set<Long> childrenSet) {
	public String toString2() {
		return String.format("<0x%8x 0x%8x 0x%8x> %10d <%10d %10d %10d> <%8.2f %8.2f> <%10s %10d %10d %10d> %s", 
			uid, sid, callSiteValue, cnt, work, tpWork, spWork, minSP, maxSP, pbit, totalChildCnt, minChildCnt, maxChildCnt, childrenSet);
	}
	
	public String toString() {
		return String.format("id: %d sid: %16x cid: %16x type: %d rtarget: %d instance: %4d pbit %s nChildren: %d nStats: %d",
				uid, sid, callSiteValue, type, recursionTarget, cnt, pbit, childrenSet.size(), statList.size());
	}
	
	TraceEntry setNumInstance(long instance) {
		this.cnt = instance;
		return this;
	}
	
	TraceEntry addChild(long id) {
		childrenSet.add(id);
		return this;
	}
	
	TraceEntry setPBit(boolean p) {
		this.pbit = p;
		return this;
	}
	
	TraceEntry addStat(CRegionStat stat) {
		statList.add(stat);
		return this;
	}
	
	TraceEntry setRecursionTarget(long id) {
		assert(type == 2);
		recursionTarget = id;
		return this;
	}
}

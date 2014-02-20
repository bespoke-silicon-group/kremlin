package kremlin;
import java.util.*;

public class TraceEntry {
	long uid; // unique id for CRegion
	long sid; // id of correspondinging static region
	long callsiteID; // callsite id
	long type; // R_INIT, R_SINK, or NORMAL
	long cnt; // number of instances
	long work;

	double minSP, maxSP;
	long recursionTarget;
	long totalChildCnt, minChildCnt, maxChildCnt;
	boolean pbit;
	Set<Long> childrenSet;
	List<CRegionStat> statList;
	
	public TraceEntry(long uid, long sid, long callsiteID, long type) {	
		this.uid = uid;
		this.sid = sid;
		this.callsiteID = callsiteID;
		//this.cnt = cnt;		
		this.childrenSet = new HashSet<Long>();
		this.statList = new ArrayList<CRegionStat>();
		this.type = type;
		this.recursionTarget = 0;
	}

	public String toString() {
		return String.format("id: %d sid: %16x cid: %16x type: %d rtarget: %d instance: %4d pbit %s nChildren: %d nStats: %d",
				uid, sid, callsiteID, type, recursionTarget, cnt, pbit, childrenSet.size(), statList.size());
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

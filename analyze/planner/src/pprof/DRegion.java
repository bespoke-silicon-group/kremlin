package pprof;
import java.util.*;

public class DRegion implements Comparable{
	long did;
	SRegion sregion;
	DRegion parent;	
	long cpLength;
	long startTime, endTime;
	long pSid, pDid;
	Set<DRegion> children;
	
	DRegion(long id, long start, long end, long cp, SRegion sregion, long pSid, long pDid) {
		this.did = id;
		this.startTime = start;
		this.endTime = end;
		this.cpLength = cp;
		this.sregion = sregion;
		this.children = new HashSet<DRegion>();
		this.parent = null;
		this.pSid = pSid;
		this.pDid = pDid;
		
	}
	
	public String toString() {		
		return String.format("D[%3d:%3d][%s][%d-%d]:%5d %5d", 
				sregion.id, did, sregion.module, sregion.startLine, sregion.endLine, cpLength, getWork());
	}
	
	void setParent(DRegion parent) {
		this.parent = parent;
	}
	
	void addChild(DRegion child) {
		children.add(child);
	}
	
	long getWork() {
		return endTime - startTime;
	}

	@Override
	public int compareTo(Object o) {
		DRegion target = (DRegion)o;
		if (target.sregion != this.sregion)
			return (int)(this.sregion.id - target.sregion.id);
		else {
			long diff = this.did - target.did;
			if (diff > 0)
				return 1;
			else if (diff < 0)
				return -1;
			else
				return 0;
		}
	}
}

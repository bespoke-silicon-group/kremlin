package predictor;
import planner.*;
import pprof.*;

public class PElement implements Comparable {
	SRegionInfo info;
	long serialTime;
	long parallelTime;
	
	PElement(SRegionInfo info, long time) {
		this.info = info;
		this.serialTime = time;
		this.parallelTime = time;
	}
	
	public SRegionInfo getSRegionInfo() {
		return info;
	}
	
	public long getSerialTime() {
		return serialTime;
	}
	
	public long getParallelTime() {
		return parallelTime;
	}
	
	void parallelize(double speedup) {
		this.parallelTime = (long)(this.serialTime / speedup);
	}

	@Override
	public int compareTo(Object o) {
		long time = ((PElement)o).serialTime;
		long diff = time - this.serialTime;
		
		if (diff > 0)
			return 1;
		else if (diff < 0)
			return -1;
		else
			return 0;
	}
	
	public String toString() {
		return String.format("%s [%d\t%d]", info.getSRegion(), serialTime, parallelTime);
	}
}

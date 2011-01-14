package target;

import java.util.*;

import pprof.SRegionInfo;

public class TargetCilkSimple implements ITarget {

	@Override
	public List<SpeedupRecord> getSpeedupList(SRegionInfo info, int maxCore) {
		List<SpeedupRecord> ret = new ArrayList<SpeedupRecord>();
		SpeedupRecord record = getSpRecord(info, maxCore);
		ret.add(record);
		return ret;
	}
	
	private SpeedupRecord getSpRecord(SRegionInfo info, int maxCore) {
		double sp = info.getSelfParallelism();
		if (maxCore < sp)
			sp = maxCore;
		return new SpeedupRecord("SP", (int)sp, sp);
	}

}

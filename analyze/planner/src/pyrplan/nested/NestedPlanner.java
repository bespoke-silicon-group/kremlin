package pyrplan.nested;

import java.util.*;

import pprof.*;
import pyrplan.*;
import pyrplan.backward.ProgramStatus;

public class NestedPlanner extends Planner {
	int numCore;
	boolean allowNesting;
	Map<SRegionInfo, Integer> coreMap = new HashMap<SRegionInfo, Integer>();
	RegionTimeStatus timeStatus;
	int overhead;
	/*
	class RecommendUnit {
		SRegionInfo info;
		double speedup;
		double timeSave;
		int nCore;		
		RecommendUnit(SRegionInfo info, int nCore, double speedup, double timeSave) {
			this.info = info;
			this.nCore = nCore;
			this.speedup = speedup;
			this.timeSave = timeSave;
		}
		
		public String toString() {
			return String.format("%s core=%d speedup=%.2f", info.getSRegion(), nCore, speedup);
		}
	}*/
	
	public NestedPlanner(SRegionInfoAnalyzer analyzer, int numCore, int overhead, boolean allowNesting) {
		super(analyzer);
		this.numCore = numCore;
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			coreMap.put(each, numCore);
		}
		this.timeStatus = new RegionTimeStatus(analyzer);
		this.overhead = overhead;
		this.allowNesting = allowNesting;
	}

	RegionRecord getEstimatedExecTime(SRegionInfo info, int maxCore) {
		double sp = info.getSelfParallelism();
		//double speedup = (sp > numCore) ? maxCore : sp;
		//double overhead = 1000.0;
		double parallelTime = (info.getAvgWork() + overhead) / sp;
		double speedup = info.getAvgWork() / parallelTime;
		if (speedup > maxCore)
			speedup = maxCore;
		if (speedup < 1.0)
			speedup = 1.0;
		int nCore = (int)Math.ceil(speedup);
		RegionRecord unit = new RegionRecord(info, nCore, speedup);
		assert(unit.getSpeedup() >= 1.0);
		long expectedTime = timeStatus.peekParallelTime(unit);
		unit.setExpectedExecTime(expectedTime);
		double timeSave = info.getTotalWork() - info.getTotalWork() / speedup;
		//return info.getTotalWork() - info.getTotalWork() / speedup;
		
		return unit;
	}
	
	int getMaxAvailableCore(SRegionInfo info) {
		int nCore = coreMap.get(info);
		return nCore;
	}
	
	RegionRecord pickBest(Set<SRegionInfo> set, long prevTime) {
		//double maxTimeSavings = analyzer.getRootInfo().getTotalWork() * 0.01;
		long threshold = (long)(prevTime * 0.03);
		long minExpectedExecTime = prevTime;
		RegionRecord best = null;
		int useCore = 1;
		
		for (SRegionInfo each : set) {
			int nCore = getMaxAvailableCore(each);			
			RegionRecord unit = getEstimatedExecTime(each, nCore);				
			//System.out.println("\t[peek] " + unit);
			
			if (minExpectedExecTime > unit.getExpectedExecTime()) {
				minExpectedExecTime = unit.getExpectedExecTime();
				best = unit;
			}
		}
		if (prevTime - minExpectedExecTime > threshold)
			return best;
		else
			return null;
	}
	
	void updateNode(SRegionInfo info, int coresTaken, Set<SRegionInfo> readySet) {
		int nCore = coreMap.get(info);
		int updatedCore = nCore / coresTaken;
		if (!allowNesting)
			updatedCore = 1;
		
		if (updatedCore <= 1) {
			readySet.remove(info);
			//System.out.println("\t[remove] " + info.getSRegion());
		}
		//System.out.println("updating core # to " + updatedCore + " at " + info.getSRegion());
		coreMap.put(info, updatedCore);
	}
	
	void updateStatus(RegionRecord unit, Set<SRegionInfo> readySet, Map<SRegionInfo, Integer> coreMap) {		
		Set<SRegionInfo> retiredSet = new HashSet<SRegionInfo>();
		
		List<SRegionInfo> updateListUp = new ArrayList<SRegionInfo>();
		updateListUp.add(unit.getRegionInfo());		
		while (!updateListUp.isEmpty()) {
			SRegionInfo current = updateListUp.remove(0);
			retiredSet.add(current);			
						
			Set<SRegion> parents = current.getParents();
			for (SRegion parent : parents) {
				SRegionInfo parentInfo = analyzer.getSRegionInfo(parent);
				updateNode(parentInfo, unit.getCoreCount(), readySet);
				if (!retiredSet.contains(parentInfo))
					updateListUp.add(parentInfo);				
			}
		}
		
		List<SRegionInfo> updateListDown = new ArrayList<SRegionInfo>();
		updateListDown.add(unit.getRegionInfo());
		while (!updateListDown.isEmpty()) {
			SRegionInfo current = updateListDown.remove(0);
			//System.out.println("current = " + current.getSRegion());
			retiredSet.add(current);
			Set<SRegion> children = current.getChildren();
			//removeList.addAll(current.getChildren());
			for (SRegion child : children) {
				SRegionInfo childInfo = analyzer.getSRegionInfo(child);
				updateNode(childInfo, unit.getCoreCount(), readySet);
				if (!retiredSet.contains(childInfo))
					updateListDown.add(childInfo);				
			}	
		}
		//assert(false);
	}
	
	private Set<SRegionInfo> getExcludeSet(SRegionInfoAnalyzer analyzer, FilterControl filter) {		
		SRegionManager sManager = analyzer.getSManager();
		double minDOALL = filter.getMinDOALL();
		double minDOACROSS = filter.getMinDOACROSS();
		double minSP = filter.getMinSP();
		

		Set<SRegion> loopSet = sManager.getSRegionSet();
		Set<SRegionInfo> loopInfoSet = SRegionInfoFilter.toSRegionInfoSet(analyzer, loopSet);
		Set<SRegionInfo> filteredInfoSet = SRegionInfoFilter.filterMinSpeedupAllLoop(loopInfoSet, minDOALL, minDOACROSS, minSP);
		String file = filter.getFileName();
		List<SRegionInfo> doAllInfoSet = SRegionInfoFilter.filterExcludedRegions(analyzer, new ArrayList<SRegionInfo>(filteredInfoSet), file);
		//List<SRegion> filteredSet = SRegionInfoFilter.toSRegionList(doAllInfoSet);
		//Set<SRegion> ret = sManager.getSRegionSet();
		//ret.removeAll(filteredSet);	
		//Set<SRegion> ret = new HashSet<SRegion>();
		Set<SRegionInfo> ret = analyzer.getSRegionInfoSet();
		ret.removeAll(doAllInfoSet);
		return ret;
	}
	
	public List<RegionRecord> plan(Set<SRegionInfo> toBeFiltered) {
		//System.out.printf("DP Planner Out MIN_SPEEDUP=%.3f MIN_SP=%.3f OUTER_INCENTIVE=%.2f\n", minSpeedup, minSP, outerIncentive);
		//Set<SRegionInfo> excludeSet = getExcludeSet(analyzer, filter);
		List<RegionRecord> ret = new ArrayList<RegionRecord>();		
		//ProgramStatus status = new ProgramStatus(analyzer, excludeSet);
		Set<SRegionInfo> readySet = new HashSet<SRegionInfo>();
		
		for (SRegionInfo info : coreMap.keySet()) {
			/*
			if (info.getSRegion().getType() == RegionType.BODY)
				continue;
			if (info.getSRegion().getType() == RegionType.FUNC)
				continue;*/			
			if (info == analyzer.getRootInfo())
				continue;
			
			readySet.add(info);
		}
		readySet.removeAll(toBeFiltered);
		//readySet.removeAll(excludeSet);
		long lastTime = analyzer.getRootInfo().getTotalWork();

		while (!readySet.isEmpty()) {
			RegionRecord unit = pickBest(readySet, lastTime);
			//System.out.println("!!!" + unit);
			if (unit == null)
				break;
			//assert(false);
			timeStatus.parallelize(unit);
			updateStatus(unit, readySet, coreMap);
			readySet.remove(unit.getRegionInfo());
			lastTime = unit.getExpectedExecTime();
			//ret.add(new RegionRecord(unit.getRegionInfo()));
			ret.add(unit);
			
		}				
		return ret;
	}	
}

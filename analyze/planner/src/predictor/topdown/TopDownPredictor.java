package predictor.topdown;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import pprof.*;
import pyrplan.*;

public class TopDownPredictor {
	//SRegionInfoAnalyzer analyzer;
	CRegionManager analyzer;
	Map<CRegion, InfoStatus> infoMap = new HashMap<CRegion, InfoStatus>();
	CRegionTimeStatus timeStatus;
	boolean uniquified;
	boolean useIntegerCore;
	
	class Unit {
		double speedup;
		double core;
		Unit(double speedup, double core) {
			this.speedup = speedup;
			this.core = core;
		}
	}
	
	public TopDownPredictor(CRegionManager analyzer, boolean useIntCore) {
		this.analyzer = analyzer;
		this.uniquified = true;
		this.useIntegerCore = useIntCore;
		
	}
	
	double getMaxAvailableCore(CRegion info) {
		return infoMap.get(info).maxCore;
	}
	
	void setStatusDoubleCore(InfoStatus status, double maxCore, double speedup) {
		assert(speedup <= maxCore);
		status.allocatedCore = speedup;
		status.speedup = speedup;		
	}
	
	void setStatusIntCore(InfoStatus status, int maxCore, double speedup) {
		double allocatedCore = 1.0;
		assert(speedup >= 1.0);		
		if (maxCore > speedup) {
			allocatedCore = Math.round(speedup);	
			if (allocatedCore < speedup)
				speedup = allocatedCore;
			//System.out.println("allocatedCore = " + allocatedCore + " speedup=" + speedup);
		} else {
			allocatedCore = Math.floor(speedup);
			speedup = allocatedCore;
			//System.out.println("!allocatedCore = " + allocatedCore + " speedup=" + speedup);
		}
		assert(speedup <= allocatedCore);
		status.allocatedCore = allocatedCore;
		status.speedup = speedup;
		
	}
	
	boolean isLeafFuncOrLoop(CRegion info) {
		/*
		RegionType type = info.getSRegion().getType();
		if (type == RegionType.BODY)
			return info.isLeaf();
		else if (type == RegionType.LOOP) {
			if (info.getChildren().size() == 0)
				return true;
			
			SRegionInfo bodyInfo = analyzer.getSRegionInfo(info.getChildren().iterator().next());
			return bodyInfo.isLeaf();
		} else*/
			return false;			
	}
	
	Unit getBestUnit(CRegion info, double maxCore, double overhead, boolean intCore) {
		if (info.getAvgWork() == 0)
			return new Unit(1.0, 1);
		
		double sp = info.getSelfParallelism();
		double spSpeedup = (maxCore > sp) ? sp : maxCore;
		double core = spSpeedup;		
		if (intCore) {			
			core = Math.floor(spSpeedup + 0.00001);
			int nextCore = (int)Math.floor((maxCore+0.00001) / 2);
			
			if (!isLeafFuncOrLoop(info) &&
					core < (maxCore-0.9) && core > nextCore - 0.5) {
				System.out.printf("from %.2f to %d where max %.2f\n", core, nextCore, maxCore);
				core = nextCore;
				
			}
			spSpeedup = core;
			
			double less = core - Math.floor(core);
			assert(less < 0.0001);
		}		
		double parallelTime = info.getAvgWork() / spSpeedup + overhead;
		double speedup = info.getAvgWork() / parallelTime;
		
		if (speedup > (sp+0.0001) || speedup > core+0.001) {
			System.out.printf("speedup = %.2f, sp = %.2f\n", speedup, sp);
			assert(false);
		}
		assert(core <= maxCore);
		
		
		if (speedup < 1.0) {
			speedup = 1.0;
			core = 1.0;
		}	
		return new Unit(speedup, core);
	}
	
	
	void parallelize(CRegion info, double overhead) {		
		double maxCore = getMaxAvailableCore(info);
		
		Unit unit = getBestUnit(info, maxCore, overhead, this.useIntegerCore);	

		InfoStatus status = infoMap.get(info);
		status.speedup = unit.speedup;
		status.allocatedCore = unit.core;	
		/*	
		System.out.printf("[%5.2f @ %5.2f sp=%5.2f] %s\n", 
				status.speedup, status.allocatedCore, info.getSelfParallelism(), info.getSRegion());*/
		
		if (status.speedup > 1.000001) {						
			timeStatus.parallelize(info, status.speedup);
		} else {
			assert(status.speedup > 0.999);
			assert(status.speedup < 1.001);
			//assert(status.allocatedCore == 1.0);
		}
	}
	
	void init(int maxCore) {
		this.timeStatus = new CRegionTimeStatus(analyzer);
		for (CRegion each : analyzer.getCRegionSet()) { 
			infoMap.put(each, new InfoStatus(maxCore));
		}
	}
	
	void setMaxCore(CRegion info, double core) {
		InfoStatus status = infoMap.get(info);
		assert(core >= 1.0);
		status.maxCore = core;
	}
	
	double getChildMaxCore(CRegion info) {
		InfoStatus status = infoMap.get(info);
		double childMaxCore = status.maxCore / status.allocatedCore;
		if (childMaxCore < 0.99999) {
			System.out.println("max=" + status.maxCore + "  allocated=" + status.allocatedCore);
		}
		assert(childMaxCore >= 0.99999);
		return childMaxCore + 0.0000001;
	}
	
	public double predict(int maxCore, Set<CRegion> toExclude, double overhead) {
		init(maxCore);
		List<CRegion> readyList = new ArrayList<CRegion>();
		Set<CRegion> retired = new HashSet<CRegion>();
		
		readyList.add(analyzer.getRoot());
		while(!readyList.isEmpty()) {
			CRegion current = readyList.get(0);
			
			if (!toExclude.contains(current)) {
				//System.out.println("Current = " + current);
				parallelize(current, overhead);
			}
			retired.add(current);
			readyList.remove(current);						
			double childMaxCore = getChildMaxCore(current);			
			
			Set<CRegion> children = current.getChildrenSet();
			for (CRegion child : children) {				 
				setMaxCore(child, childMaxCore);
				/*
				if (retired.contains(childInfo)) {
					assert(childInfo.getParents().size() > 1);
				}*/
				
				assert(!retired.contains(child));
				assert(!readyList.contains(child));
				readyList.add(child);								
			}						
		}
		double parallelTime = timeStatus.getExecTime();
		double serialTime = analyzer.getRoot().getTotalWork();
		return serialTime / parallelTime;
	}	
	
	
}
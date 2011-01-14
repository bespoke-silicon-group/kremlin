package predictor.predictors;
import java.util.*;
import pprof.*;
import pprof.SRegionSpeedupCalculator.LimitFactor;
import pprof.SRegionSpeedupCalculator.ScaleMode;

import target.*;

public class PerfPredictor {
	URegionManager manager;
	
	public PerfPredictor(URegionManager manager) {
		this.manager = manager;
	}

	//public void predict(List<RegionRecord> list, double minWorkChunk, double satBw) {
	public void predict(List<SRegionInfo> list, double minWorkChunk, double satBw, String outFile) {
		SRegionSpeedupCalculator speedupCalc = new SRegionSpeedupCalculator
			(ScaleMode.LINEAR, LimitFactor.ALL,	minWorkChunk, satBw);

		//Set<SRegionInfo> set = new HashSet<SRegionInfo>();
		//for (RegionRecord record : list) {
		//	set.add(record.info);
		//}
		System.out.printf("Min Work Chunk = %.2f, Saturation Bandwidth = %.2f\n",
				minWorkChunk, satBw);
		
/*
		int cores[] = {1, 2, 4, 8, 16, 32, -1};
		for (int i=0; i<cores.length; i++) {
			int coreNum = cores[i];
			double speedup = speedupCalc.getAppSpeedup(new HashSet<SRegionInfo>(list), coreNum);
			System.out.printf("Core=%3d, Speedup=%8.2f\n", coreNum, speedup);			
		}*/
		
		ITarget target = new TargetCilkSimple();
		//TargetCilk target = new TargetCilk();
		//TargetOmp target = new TargetOmp();
		for (int core=2; core<=32; core=core*2) {
			
			System.out.println("\n# of Core = " + core);
			for (SRegionInfo info : list) {
			/*
			double speedup = speedupCalc.getRegionSpeedup(info);
			LimitFactor factor = speedupCalc.getLimitFactor(info);
			double iter = info.getAvgIteration();
			double sp = speedupCalc.getRegionSpeedup(info, LimitFactor.SP);
			double granularity = speedupCalc.getRegionSpeedup(info, LimitFactor.GRANULARITY);
			int coreGranularity = speedupCalc.getOptimalCoreSize(info, LimitFactor.GRANULARITY);
			double bw = speedupCalc.getRegionSpeedup(info, LimitFactor.BANDWIDTH);
			int coreBw = speedupCalc.getOptimalCoreSize(info, LimitFactor.BANDWIDTH);
			
			
			System.out.printf("%s\t Speedup=%6.2f (%d, %10.2f) (%d, %10.2f) (%d, %10.2f) %10s\n", 
					info.getSRegion(), speedup, 
					(int)sp, sp, coreGranularity, granularity, coreBw, bw, factor);*/
			
			List<SpeedupRecord> result = target.getSpeedupList(info, core);
			for (SpeedupRecord each : result)
				System.out.printf("%s %s\n", info.getSRegion(), each);
			
			
				
				//for (SpeedupRecord each : result)
				//	System.out.println(each);
			
			
			
			}
		}
		
		
	}	
}

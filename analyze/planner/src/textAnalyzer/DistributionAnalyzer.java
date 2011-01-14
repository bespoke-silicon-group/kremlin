package textAnalyzer;

import pprof.RegionType;

public class DistributionAnalyzer {
	DataEntryManager manager;
	
	DistributionAnalyzer(DataEntryManager manager) {
		this.manager = manager;
	}
	
	void analyzeSP() {
		double maxSP[] = {1.1, 2.0, 5.0,  -1};
		int cnt[] = {0, 0, 0, 0, 0};
		for (DataEntry entry : manager.getList()) {
			for (int i=0; i<maxSP.length; i++) {
				double sp = entry.sp;
				double maxValue = maxSP[i];
				if (sp < maxValue || maxValue == -1) {
					cnt[i]++;
					break;
				}
			}
		}
		
		System.out.println("SP analysis");
		for (int i=0; i<maxSP.length; i++) {
			System.out.printf("max = %.2f : cnt = %d\n", maxSP[i], cnt[i]);
		}
	}
	
	void analyzeTP() {
		double maxTP[] = {1.1, 2.0, 5.0, -1};
		int cnt[] = {0, 0, 0, 0, 0};
		for (DataEntry entry : manager.getList()) {
			for (int i=0; i<maxTP.length; i++) {
				double tp = entry.tp;
				double maxValue = maxTP[i];
				if (tp < maxValue || maxValue == -1) {
					cnt[i]++;
					break;
				}
			}
		}
		
		System.out.println("TP analysis");
		for (int i=0; i<maxTP.length; i++) {
			System.out.printf("max = %.2f : cnt = %d\n", maxTP[i], cnt[i]);
		}
	}
	
	void analyzeExploited() {
		double maxTP[] = {1.1, 2.0, 5.0, -1};
		int cnt[] = {0, 0, 0, 0, 0};
		int total = 0;
		int lowCoverageCnt = 0;
		int region1Cnt = 0;
		int region1Exploited = 0;
		for (DataEntry entry : manager.getList()) {
			double coverage = entry.coverage;
			double sp = entry.sp;
			if (coverage >= 5.0 && sp >= 5.0) {
				region1Cnt++;
				if (entry.parallelized)
					region1Exploited++;
			}
			
			if (entry.parallelized) {
				total++;
				
				if (coverage < 5.0) {
					lowCoverageCnt++;
				}				
				
			}
		}
		
		System.out.printf("Exploited analysis %d / %d\n", 
				lowCoverageCnt, total);
		
		
	}
	
	void analyzeArea() {
		int regionCnt[] = {0, 0, 0, 0, 0, 0};
		int hitCnt[] = {0, 0, 0, 0, 0, 0};
		double maxTP[] = {1.1, 2.0, 5.0, -1};
		double maxCoverage[] = {1.1, 2.0, 5.0, -1};
		
		for (DataEntry entry : manager.getList()) {
			if (entry.type != RegionType.LOOP)
				continue;
			
			double coverage = entry.coverage;
			double sp = entry.sp;
			
			if (sp >= 5.0) {
				if (coverage >= 5.0) {
					regionCnt[0]++;
					if (entry.parallelized)
						hitCnt[0]++;
					
				} else if (coverage >= 1.0) {
					regionCnt[1]++;
					if (entry.parallelized)
						hitCnt[1]++;
					
				} else {
					regionCnt[2]++;
					if (entry.parallelized)
						hitCnt[2]++;
				}				
			}
			
			if (sp < 5.0) {
				if (coverage >= 5.0) {
					regionCnt[3]++;
					if (entry.parallelized)
						hitCnt[3]++;
					
				} else if (coverage >= 1.0) {
					regionCnt[4]++;
					if (entry.parallelized)
						hitCnt[4]++;
					
				} else {
					regionCnt[5]++;
					if (entry.parallelized)
						hitCnt[5]++;
				}				
			}			
						
		}
		//System.out.printf("Area1 analysis %d / %d\n", 
		//		region1Exploited, region1Cnt);
		System.out.println("Region Analysis");
		for (int i=0; i<6; i++) {
			double ratio = (double)hitCnt[i] / regionCnt[i] * 100.0;
			System.out.printf("%d / %d = %.2f\n", 
					hitCnt[i], regionCnt[i], ratio);
		}
		
	}
}

package textAnalyzer;

import java.util.*;

public class ParallelizationAnalyzer {
	DataEntryManager manager;
	ParallelizationAnalyzer(DataEntryManager manager) {
		this.manager = manager;
	}
	
	void analyzeSP(List<DataEntry> data) {
		System.out.println("\n\nSP Analysis");
		double[] coverageList = {-1, 1, 5, -1};
		double[] nspList = {-1, 1.1, 5, -1};
		//double[] coverageList = {-1, 50, -1};
		//double[] nspList = {-1, 0.5, -1};
		
		for (int i=0; i<coverageList.length-1; i++) {
			double minCoverage = coverageList[i];
			double maxCoverage = coverageList[i+1];
			List<DataEntry> filtered1 = filterByCoverage(data, minCoverage, maxCoverage);
			System.out.println("size = " + filtered1.size());
			for (int j=0; j<nspList.length-1; j++) {
				double minNsp = nspList[j];
				double maxNsp = nspList[j+1];
				List<DataEntry> filtered2 = filterBySP(filtered1, minNsp, maxNsp);
				
				System.out.printf("coverage [%.2f, %.2f], parallelism [%.2f, %.2f]\t", 
						minCoverage, maxCoverage, minNsp, maxNsp);
				analyzeList(filtered2);
			}
		}
	}
	
	void analyzeTP(List<DataEntry> data) {
		System.out.println("\n\nTP Analysis");
		double[] coverageList = {-1, 1, 5, -1};
		double[] nspList = {-1, 1.1, 5, -1};
		//double[] coverageList = {-1, 50, -1};
		//double[] nspList = {-1, 0.5, -1};
		
		for (int i=0; i<coverageList.length-1; i++) {
			double minCoverage = coverageList[i];
			double maxCoverage = coverageList[i+1];
			List<DataEntry> filtered1 = filterByCoverage(data, minCoverage, maxCoverage);
			System.out.println("size = " + filtered1.size());
			
			for (int j=0; j<nspList.length-1; j++) {
				double minNsp = nspList[j];
				double maxNsp = nspList[j+1];
				List<DataEntry> filtered2 = filterByTP(filtered1, minNsp, maxNsp);
				
				System.out.printf("coverage [%.2f, %.2f], parallelism [%.2f, %.2f]\t", 
						minCoverage, maxCoverage, minNsp, maxNsp);
				analyzeList(filtered2);
			}
		}
	}
	
	void analyzeList(List<DataEntry> data) {
		int totalCnt = data.size();
		int parallelCnt = 0;
		int groupCnt = 0;
		int ancestorCnt = 0;
		int unresolvedCnt = 0;
		
		for (DataEntry each : data) {
			if (each.parallelized) {
				parallelCnt++;
				continue;
			}
			
			if (each.isGroupParallelized) {
				groupCnt++;
				continue;
			}
			
			if (each.ancestorParallelized) {
				ancestorCnt++;
				continue;
			}
			
			unresolvedCnt++;						
		}
		
		double parallel = ((double)parallelCnt / (double)totalCnt) * 100.0;
		double group = ((double)(parallelCnt + groupCnt) / (double)totalCnt) * 100.0;
		double ancestor = ((double)(parallelCnt + groupCnt + ancestorCnt) / (double)totalCnt) * 100.0;		
		

		//System.out.printf("\ttotal: %d parallel: %.2f group: %.2f ancestor: %.2f\n",
		//		totalCnt, parallel, group, ancestor);		
		//System.out.printf("\ttotal: %d parallel: %d group: %d ancestor: %d\n",
		//		totalCnt, parallelCnt, groupCnt, ancestorCnt);
		System.out.printf("\ttotal: %d exploited: %d rate: %.2f \n",
				totalCnt, parallelCnt, (double)parallelCnt*100.0/totalCnt);
/*
		System.out.printf("\ttotal: %d parallelCnt: %d parallel: %.2f group: %.2f ancestor: %.2f\n",
				totalCnt, parallelCnt, parallel, group, ancestor);		
	*/	

	}
	
	List<DataEntry> filterByCoverage(List<DataEntry> data, double minCoverage, double maxCoverage) {
		List<DataEntry> ret = new ArrayList<DataEntry>();
		for (DataEntry each : data) {
			if (minCoverage > 0) {
				if (each.coverage < minCoverage)
					continue;
			}			
			if (maxCoverage > 0) {
				if (each.coverage > maxCoverage)
					continue;
			}			
			ret.add(each);
		}
		return ret;
	}
	
	List<DataEntry> filterByNSP(List<DataEntry> data, double minNSP, double maxNSP) {
		List<DataEntry> ret = new ArrayList<DataEntry>();
		for (DataEntry each : data) {
			if (minNSP > 0) {
				if (each.nsp < minNSP)
					continue;
			}			
			if (maxNSP > 0) {
				if (each.nsp > maxNSP)
					continue;
			}			
			ret.add(each);
		}
		return ret;
	}
	
	List<DataEntry> filterByTP(List<DataEntry> data, double minNSP, double maxNSP) {
		List<DataEntry> ret = new ArrayList<DataEntry>();
		for (DataEntry each : data) {
			if (minNSP > 0) {
				if (each.tp < minNSP)
					continue;
			}			
			if (maxNSP > 0) {
				if (each.tp >= maxNSP)
					continue;
			}			
			ret.add(each);
		}
		return ret;
	}
	
	List<DataEntry> filterBySP(List<DataEntry> data, double minNSP, double maxNSP) {
		List<DataEntry> ret = new ArrayList<DataEntry>();
		for (DataEntry each : data) {
			if (minNSP > 0) {
				if (each.sp < minNSP)
					continue;
			}			
			if (maxNSP > 0) {
				if (each.sp >= maxNSP)
					continue;
			}			
			ret.add(each);
		}
		return ret;
	}
}

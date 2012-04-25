package textAnalyzer;

import pprof.RegionType;
import java.util.*;
public class TextDataAnalyzer {
	public static void main(String[] args) {
		//DataEntryManager manager = new DataEntryManager();

		String baseDir = "f:\\work\\npb-u";
		String bench[] = {"ft", "cg", "ep", "ft", "is", "lu", "mg", "sp"};
		DataEntryManager manager = null;
		for (int i=0; i<bench.length; i++) {
			String name = baseDir + "\\" + bench[i] + "\\" + bench[i] + ".dat";
			manager = new DataEntryManager();
			manager.readFile(name);
			//manager.readFile("f:\\work\\pyrprof\\merged.dat");
			ParallelizationAnalyzer analyzer = new ParallelizationAnalyzer(manager);
			
			//List<DataEntry> start = manager.getList(RegionType.LOOP);
			List<DataEntry> start = manager.getList();
			List<DataEntry> filtered3 = analyzer.filterByCoverage(start, 0.01, -1);
			List<DataEntry> filtered4 = analyzer.filterByTP(filtered3, 5.0, -1);
			List<DataEntry> filtered5 = analyzer.filterBySP(filtered4, 5.0, -1);
			//List<DataEntry> filtered5 = analyzer.filterByCoverage(manager.getList(), -1.0, 0.01);
			System.out.printf("%s %d %d %d %d\n", 
					bench[i], start.size(), filtered3.size(), filtered4.size(), filtered5.size());
		}
		assert(false);
		 
		//manager.readFile("/h/g3/dhjeon/trunk/test/parasites/pyrprof/chartGenBack/merged.dat");

		ParallelizationAnalyzer analyzer = new ParallelizationAnalyzer(manager);
		//analyzer.analyzeSP(manager.getList(RegionType.LOOP));
		//analyzer.analyzeTP(manager.getList(RegionType.LOOP));
		
		analyzer.analyzeSP(manager.getList());
		analyzer.analyzeTP(manager.getList());
		List<DataEntry> filtered0 = analyzer.filterByCoverage(manager.getList(), -1.0, 1);
		List<DataEntry> filtered1 = analyzer.filterByCoverage(manager.getList(), 1, 5);
		List<DataEntry> filtered2 = analyzer.filterByCoverage(manager.getList(), 5, -1);
		System.out.printf("%d %d %d\n", 
				filtered0.size(), filtered1.size(), filtered2.size());
		
		List<DataEntry> start = manager.getList();
		List<DataEntry> filtered3 = analyzer.filterByCoverage(start, 0.01, -1);
		List<DataEntry> filtered4 = analyzer.filterByTP(filtered3, 5.0, -1);
		List<DataEntry> filtered5 = analyzer.filterBySP(filtered4, 5.0, -1);
		//List<DataEntry> filtered5 = analyzer.filterByCoverage(manager.getList(), -1.0, 0.01);
		System.out.printf("%d %d %d %d\n", 
				start.size(), filtered3.size(), filtered4.size(), filtered5.size());
		/*
		DistributionAnalyzer anal2 = new DistributionAnalyzer(manager);
		anal2.analyzeSP();
		anal2.analyzeTP();
		anal2.analyzeExploited();
		anal2.analyzeArea();*/
	}
}

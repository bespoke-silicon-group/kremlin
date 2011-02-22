package runner;
import planner.RefPlanner;
import planner.SRegionInfoFilter;
import pprof.*;
import pyrplan.omp.DPPlanner;

import java.io.File;
import java.util.*;

public class SensitivityTester {
	double percentSpeedupThresholds[] = {0.0, 0.01, 0.1, 0.5, 1, 5};
	SRegionInfoAnalyzer analyzer;
	DPPlanner planner;
	
	SensitivityTester(SRegionInfoAnalyzer analyzer, String excludeFile) {
		this.analyzer = analyzer;
		this.planner = new DPPlanner(analyzer, excludeFile);
		
	}
	
	void test(List<SRegion> manual) {
		
		/*
		for (double speedup : percentSpeedupThresholds) {			
			double ratioUp = speedup * 0.01 + 1.0;
			FilterControl filter = new FilterControl(5.0, ratioUp, ratioUp, 1.2);
			List<SRegion> list = planner.plan(filter);			
			List<SRegion> compactList = SRegionInfoFilter.toCompactList(list);
			
			
			//System.out.printf("speedup = %.2f size = %d, %d\n", speedup, list.size(), compactList.size());
			//System.out.printf("\tsize = %d\n", list.size());
			System.out.printf("Speeup = %.2f%%", speedup);
			comparePlan(compactList, manual);
			
		}*/
	}
	
	void comparePlan(List<SRegion> pyrprof, List<SRegion> manual) {
		Set<Integer> pyrprofSet = toIntegerSet(pyrprof);
		Set<Integer> manualSet = toIntegerSet(manual);
		
		int pyrprofSize = pyrprofSet.size();
		int manualSize = manualSet.size();
		//System.out.println(pyrprofSize + " " + pyrprofSet);
		//System.out.println(manualSize + " " + manualSet);
		
		Set<Integer> manualOnlySet = new HashSet(manualSet);
		manualOnlySet.removeAll(pyrprofSet);
		int manualOnlySize = manualOnlySet.size();
		//System.out.println(manualOnlySize + " " + manualOnlySet);
		
		Set<Integer> pyrprofOnlySet = new HashSet(pyrprofSet);
		pyrprofOnlySet.removeAll(manualSet);
		int pyrprofOnlySize = pyrprofOnlySet.size();
		System.out.println(pyrprofOnlySize + " " + pyrprofOnlySet);
		
		int overlapSize0 = manualSize - manualOnlySize;
		int overlapSize1 = pyrprofSize - pyrprofOnlySize;
		//System.out.println("size0 = " + overlapSize0 + " size1 = " + overlapSize1);
		assert(overlapSize0 == overlapSize1);
		
		
		System.out.printf("\tpyrprof (%3d, %3d, %3d), manual (%3d, %3d, %3d)\n",
				pyrprofSize, overlapSize0, pyrprofOnlySize,
				manualSize, overlapSize1, manualOnlySize);
	}
	
	Set<Integer> toIntegerSet(List<SRegion> list) {
		Set<Integer> ret = new HashSet<Integer>();
		for (SRegion each : list) {
			ret.add(each.getStartLine());
		}
		return ret;
	}
	
	public static void main(String[] args) {
		System.out.println("pyrprof ver 0.1");
		String rawDir = null;
		String project = null;
		if (args.length < 1) {			
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-u/ep";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-u/is";
			rawDir = "f:\\Work\\npb-u\\sp";

			
			String[] splitted = rawDir.split("/");
			//String[] splitted = rawDir.split("\\");
			//project = splitted[splitted.length-1];
			project = "sp";
		} else {
			rawDir = args[0];
			project = args[1];
		}

		System.out.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";
		String fullExcludeFile = rawDir + "/" + project + ".exclude";


		SRegionManager sManager = new SRegionManager(new File(sFile), false);
		URegionManager dManager = new URegionManager(sManager, new File(dFile));		
		OMPGrepReader reader = new OMPGrepReader(rawDir + "/omp.txt");
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		//analyzer.isUniquified();
		
		
		SensitivityTester tester = new SensitivityTester(analyzer, fullExcludeFile);
		RefPlanner refAnalyzer = new RefPlanner(dManager.getSRegionAnalyzer());
		List<SRegionInfo> manualList = refAnalyzer.recommend(0.99);
		List<SRegion> list = SRegionInfoFilter.toSRegionList(manualList);
			
		tester.test(SRegionInfoFilter.toCompactList(list));
	}
}

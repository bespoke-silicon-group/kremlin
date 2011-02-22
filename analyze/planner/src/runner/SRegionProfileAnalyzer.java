package runner;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.*;

import javax.swing.tree.DefaultMutableTreeNode;

import planner.FilterControl;
import planner.ParallelFileReader;
import planner.ParameterSet;
import planner.RefPlanner;
import pprof.*;
import pprof.SRegionSpeedupCalculator.LimitFactor;
import pprof.SRegionSpeedupCalculator.ScaleMode;
import predictor.predictors.PerfPredictor;
import visualizer.PloticusChartGenerator;

public class SRegionProfileAnalyzer {
	SRegionManager sManager;
	SRegionInfoAnalyzer analyzer;
	OMPGrepReader reader;
	Set<SRegion> parentParallelized;
	SRegionInfoGroupManager groupManager = null;
	
	
	public SRegionProfileAnalyzer(URegionManager manager, OMPGrepReader reader) {
		this.sManager = manager.getSRegionManager();
		this.analyzer = manager.getSRegionAnalyzer();
		this.reader = reader;
		this.parentParallelized = new HashSet<SRegion>();
		
		List<SRegionInfo> workList = new ArrayList<SRegionInfo>();
		Set<SRegionInfo> retired = new HashSet<SRegionInfo>();
		for (SRegionInfo each : analyzer.getSRegionInfoSet()) {
			SRegion region = each.getSRegion();			
			if (isParallelized(region)) {
				each.setParallelStatus(RegionStatus.PARALLEL);
				workList.add(each);				
			}
		}
		
		while (workList.isEmpty() == false) {
			SRegionInfo toRemove = workList.remove(0);
			for (SRegion child : toRemove.getChildren()) {
				SRegionInfo info = analyzer.getSRegionInfo(child);
				if (!retired.contains(info)) {
					workList.add(info);
				}
			}
			this.parentParallelized.add(toRemove.getSRegion());
			retired.add(toRemove);
		}
		
		this.groupManager = new SRegionInfoGroupManager(analyzer);
	}
	
	boolean isParallelized(SRegion sregion) {
		return (sregion.getType() == RegionType.LOOP) &&
			reader.contains(sregion.getStartLine());
	}
	
	boolean parentParallelized(SRegionInfo info) {
		if (isParallelized(info.getSRegion()))
			return false;
		
		return parentParallelized.contains(info.getSRegion());
	}
	
	void printHeader(BufferedWriter output) throws Exception {
		output.write("===========================================================" +
		"===========================================================================================" +
		"===================\n");
		output.write(String.format("%10s %15s [start-end] %10s\t%10s %10s %10s %10s %10s %15s %10s %10s %10s %10s %10s \n", 
				"[pyr, gprof]", "file", "type(leaf?)", "speedup(%)", "selfP", "time(%)", "nsp", "avg cp", "avg work", "totalP",  
				"exploited", "iter", "fPositive", "fNegative"));
		output.write("===========================================================" +
		"============================================================================================" +
		"===================\n");
	}
	
	void reportFalsePositiveRegions(String fileName) {
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(fileName));			
			java.util.List<SRegionInfo> list = analyzer.getPyrprofList(RegionType.LOOP);
			java.util.List<SRegionInfo> listGprof = analyzer.getGprofList(RegionType.LOOP);
			
			printHeader(output);			
			
			int i = 0;
			int cntGroup = 0;
			int cntAncestor = 0;
			int cnt = 0;
			
			
			for (SRegionInfo each : list) {
				
								
				//boolean fPositive = isFalsePositive(each.getSelfParallelism(), each.getCoverage(), 
				//		exploited, ancestorExploited);
				boolean fPositive = isFalsePositive(each);
				int pyrprofOrder = list.indexOf(each);
				int gprofOrder = listGprof.indexOf(each);				
								
				if (fPositive) {
					cnt++;
					boolean isGroupParallelized = groupManager.getGroup(each).isGroupParallelized();
					boolean ancestor = this.parentParallelized(each);
					if (isGroupParallelized)
						cntGroup++;
					else if (ancestor)
						cntAncestor++;
					else {
						String str = String.format("[%3d, %3d] %s\n", pyrprofOrder, gprofOrder, formatEntry(each));	
						output.write(str);
					}
				}
			}
			
			output.write("total # of regions: " + list.size() + "\n");
			output.write("total # of false positives: " + cnt + "\n");
			output.write("total # of group parallelized: " + cntGroup + "\n");
			output.write("total # of ancestor parallelized: " + cntAncestor + "\n");
			output.write("total # of unidentified false positives: " + (cnt - cntAncestor - cntGroup) + "\n");
			output.close();					
		
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}		
		
	}
	
	void reportFalseNegativeRegions(String fileName) {
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(fileName));			
			java.util.List<SRegionInfo> list = analyzer.getPyrprofList(RegionType.LOOP);
			java.util.List<SRegionInfo> listGprof = analyzer.getGprofList(RegionType.LOOP);
			
			printHeader(output);			
			
			int i = 0;
			for (SRegionInfo each : list) {
				SRegion sregion = each.getSRegion();
				boolean exploited = each.isExploited();
				boolean ancestorExploited = this.parentParallelized(each);
				boolean fNegative = isFalseNegative(each.getSelfParallelism(), exploited);
				int pyrprofOrder = list.indexOf(each);
				int gprofOrder = listGprof.indexOf(each);
				if (fNegative) {
					String str = String.format("[%3d, %3d] %s\n", pyrprofOrder, gprofOrder, formatEntry(each));	
					output.write(str);
				}
			}
			output.write("total # of regions: " + list.size() + "\n");
			output.close();		
		
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}		
		
	}

	
	void dumpGroup(String fileName) {
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(fileName));			
			java.util.List<SRegionInfo> list = new ArrayList<SRegionInfo>();
			printHeader(output);
					
			for (SRegion region : sManager.getSRegionSet()) {
				SRegionInfo info = analyzer.getSRegionInfo(region);
				if (info != null)
					list.add(info);
			}
			
			List<SRegionInfoGroup> groupList = groupManager.getPyrprofList();
			List<SRegionInfoGroup> gprofList = groupManager.getGprofList();
			
			int i = 0;
			for (SRegionInfoGroup each : groupList) {
				
				int gprofOrder = gprofList.indexOf(each);
				String str = String.format("[%3d, %3d] %s\n", i++, gprofOrder, formatEntry(each.getEntry(0)));
				output.write(str);
				for (int j=1; j<each.size(); j++) {
					str = String.format("           %s\n", formatEntry(each.getEntry(j)));
					output.write(str);
				}
				output.write("\n");
			}
		
			String out0 = String.format("\n\n# of static regions: %d\n", sManager.getSRegionSet().size());
			String out1 = String.format("# of static regions executed: %d\n", list.size());
			String out2 = String.format("# of static regions manually parallelized: %d\n", reader.size());
			output.write(out0);
			output.write(out1);
			output.write(out2);
			//writeSummary(output);
			output.close();		
		
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}		
		
	}	
			

	
	public void dump(String fileName) {
		try {
			BufferedWriter output =  new BufferedWriter(new FileWriter(fileName));			
			//java.util.List<SRegionInfo> list = new ArrayList<SRegionInfo>();
			printHeader(output);
			/*		
			for (SRegion region : sManager.getSRegionSet()) {
				SRegionInfo info = analyzer.getSRegionInfo(region);
				if (info != null)
					list.add(info);
			}			
			Collections.sort(list);*/
			java.util.List<SRegionInfo> listPyrprof = analyzer.getPyrprofList();
			java.util.List<SRegionInfo> listGprof = analyzer.getGprofList();
			int i = 0;
			for (SRegionInfo each : listPyrprof) {		
				//if (each.getSRegion().getType() == RegionType.BODY)
				//	continue;
				
				int gprofOrder = listGprof.indexOf(each);
				String str = String.format("[%3d, %3d] %s\n", i++, gprofOrder, formatEntry(each));
				output.write(str);
				
			}
		
			String out0 = String.format("\n\n# of static regions: %d\n", sManager.getSRegionSet().size());
			String out1 = String.format("# of static regions executed: %d\n", listPyrprof.size());
			String out2 = String.format("# of static regions manually parallelized: %d\n", reader.size());
			output.write(out0);
			output.write(out1);
			output.write(out2);
			//writeSummary(output);
			output.close();		
		
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}		
		
	}	
	
	
	String formatSRegion(SRegion region) {
		String type = region.getType().toString();
		return String.format("%15s.c [%4d-%4d] %s", 
				region.getModule(), region.getStartLine(), region.getEndLine(), type);
	}
	
	
	boolean isFalsePositive(SRegionInfo info) {
		boolean exploited = info.isExploited();
		assert(groupManager.getGroup(info) != null);
		//boolean isGroupParallelized = groupManager.getGroup(info).isGroupParallelized();
		//boolean ancestor = this.parentParallelized(info);
		double selfP = info.getSelfParallelism();
		double work = info.getCoverage();
		
		//if (exploited == true || ancestor == true || isGroupParallelized == true)
		if (exploited == true)
			return false;
		
		if (selfP >= 4.0 && work >= 1.0) 
			return true;
		
		return false;
		
	}
	/*
	boolean isFalsePositive(double selfP, double work, boolean exploited, boolean ancestor) {
		if (exploited == true || ancestor == true)
			return false;
		
		if (selfP >= 4.0 && work >= 1.0) 
			return true;
		
		return false;
	}*/
	
	boolean isFalseNegative(double selfP, boolean exploited) {
		if (exploited == false)
			return false;
		
		if (selfP < 4.0) 
			return true;
		
		return false;
	}
	
	String formatInfoEntry(SRegionInfo info) {
		StringBuffer buffer = new StringBuffer();				
		boolean exploited = info.isExploited();		
		
		
		String numbers = String.format("(%5s)\t%10.2f (%10.2f %10.2f) %10.2f %10.2f %10.2f %15.2f %10.2f (%5.2f %5.2f %5.2f) (%5.2f %5.2f %5.2f) %10s",
				info.isLeaf(), info.getSelfSpeedup(), info.getSelfParallelism(), info.getAvgIteration(), 
				info.getCoverage(), info.getNSP(),   
				info.getAvgCP(), info.getAvgWork(), info.getTotalParallelism(), 
				info.getMemReadRatio()*100, info.getMemReadLineRatio()*100, info.getAvgMemReadCnt(), 
				info.getMemWriteRatio()*100, info.getMemWriteLineRatio()*100, info.getMemWriteRatio() / info.getMemWriteLineRatio(),
				exploited				 
				);
		buffer.append(numbers);
		return buffer.toString();
	}
	
	String formatEntry(SRegionInfo info) {
		StringBuffer buffer = new StringBuffer();
		SRegion sregion = info.getSRegion();		
		buffer.append(formatSRegion(sregion));
		boolean exploited = info.isExploited();
		boolean ancestorExploited = this.parentParallelized(info);
		//boolean fPositive = isFalsePositive(info.getSelfParallelism(), info.getCoverage(), 
		//		exploited, ancestorExploited);
		boolean fPositive = isFalsePositive(info);
		
		boolean fNegative = isFalseNegative(info.getSelfParallelism(), exploited);		
		buffer.append(formatInfoEntry(info));
		double avgIteration = info.getAvgIteration();
		
		String numbers = String.format("%10.2f %10s %10s", avgIteration, fPositive, fNegative);
		buffer.append(numbers);
		return buffer.toString();
	}
	
	//0: loop, parallelized
	//1: loop, not parallelized
	//2: loop, parent parallelized
	//3: func, parallelized
	//4: func, not parallelized,
	//5: func, parent parallelized
	
	int getType(SRegionInfo info) {
		double sp = info.getSelfParallelism();
		double coverage = info.getCoverage();
		SRegion sregion = info.getSRegion();
		boolean exploited = info.isExploited();
		//boolean promising = isPromising(sp, coverage);
		boolean isFunc = sregion.isFunction();		
		boolean isParentParallelized = parentParallelized(info);
		
		if (isFunc) {
			if (exploited)
				return 0;
			else if (isParentParallelized)
				return 2;
			else
				return 1;
			
		} else {
			if (exploited)
				return 3;
			else if (isParentParallelized)
				return 5;
			else
				return 4;
		}
	}
	
	// what to write?
	
	// 1. region type
	// 2. total parallelism
	// 3. self parallelism
	// 4. exec coverage
	// 5. speedup (%) 
	// 6. manually parallelized?
	// 7. ancestor parallelized?
	// 8. extra code
	
	String getEntryString(SRegionInfo each) {
		double sp = each.getSelfParallelism();
		double tp = each.getTotalParallelism();
		double coverage = each.getCoverage();
		SRegion sregion = each.getSRegion();
		double speedup = each.getSelfSpeedup();
		int parallelized = (each.isExploited()) ? 1 : 0;
		int groupParallelized = groupManager.getGroup(each).isGroupParallelized() ? 1 : 0;
		int ancestor = this.parentParallelized(each) ? 1 : 0;
		int regionCode = sregion.getType().getCode();
		int isLeaf = (each.isLeaf() == true) ? 1 : 0;
		int type = getType(each); 
		assert(sp != 0.0);
		double nsp = 1.0 - (1.0 / sp);
		double upSelf = 1.0 / sp;
		double upTotal = 1.0 / tp;
		
		String out = String.format("%d\t%.2f\t%.2f\t%.2f\t%.2f\t%d\t%d\t%d\t%.2f\t%s\t%d\t%d\t%d\t%d\t%.2f\t%.2f\n", 
				regionCode, tp, sp, coverage, speedup, parallelized, ancestor, type, nsp,
				sregion.getFuncName(), sregion.getStartLine(), sregion.getEndLine(), isLeaf, groupParallelized,
				upTotal, upSelf);
		return out;
	}
	
	public void writeDatFile(String fileName) {
		try {
			BufferedWriter output = new BufferedWriter(new FileWriter(fileName));
			
			Set<SRegionInfo> set = analyzer.getSRegionInfoSet();
			for (SRegionInfo each : set) {
				String out = getEntryString(each);
				output.append(out);
			}
			
			output.close();
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	int fromBoolean(boolean value) {
		return (value == true) ? 1 : 0;
	}
	
	String getRelString(SRegionInfo info) {
		boolean exploited = info.isExploited();
		boolean ancestorExploited = this.parentParallelized(info);
		//assert(exploited == false && ancestorExploited == true);
		
		Set<SRegion> directParents = info.getParents();
		assert(directParents.size() == 1);
		Set<SRegion> parents = new HashSet<SRegion>();
		
		for (SRegion parent : directParents) {
			SRegion current = parent;
			while(true) {
				if (current.getType() == RegionType.LOOP) {
					parents.add(current);
					break;
					
				} else if (current.getType() == RegionType.FUNC) {
					parents.add(current);
					break;
					
				} else {
					SRegionInfo pInfo = analyzer.getSRegionInfo(parent);
					SRegion first = pInfo.getParents().iterator().next();
					current = first;
				}
			}
		}
		
		
		StringBuffer buffer = new StringBuffer();
		for (SRegion parent : parents) {
			SRegionInfo parentInfo = analyzer.getSRegionInfo(parent);
			String ret = String.format("%.2f\t%.2f\t%d\t%.2f\t%.2f\t%d\n", 
				parentInfo.getSelfParallelism(), parentInfo.getCoverage(), fromBoolean(isParallelized(parent)),
				info.getSelfParallelism(), info.getCoverage(), fromBoolean(isParallelized(info.getSRegion())));
			buffer.append(ret);			
		}	
		
		return buffer.toString();		
	}
	
	public void writeRelFile(String fileName) {
		try {
			BufferedWriter output = new BufferedWriter(new FileWriter(fileName));
			
			Set<SRegionInfo> set = analyzer.getSRegionInfoSet();
			for (SRegionInfo each : set) {
				SRegion region = each.getSRegion();
				if (region.getType() != RegionType.LOOP)
					continue;
				
				boolean exploited = isParallelized(each.getSRegion());
				boolean ancestorExploited = this.parentParallelized(each);				
				
				if (exploited == false && ancestorExploited == false) {
					continue;					
				}
				
				String out = getRelString(each);
				output.append(out);
			}
			
			output.close();
		} catch(Exception e) {
			e.printStackTrace();
			assert(false);
		}
	}
	
	public static void printEstimatedSpeedups(String title, SRegionSpeedupCalculator calc, List<SRegionInfo> list) {		
		int coreArray[] = {1, 2, 4, 8, 16, 32};
		
		System.out.println("\n" + title);
		Map<Integer, Double> perfMap = new HashMap<Integer, Double>();
		for (int core : coreArray) {
			double speedup = calc.getAppSpeedup(new HashSet<SRegionInfo>(list), core);
			perfMap.put(core, speedup);			
		}	
		double speedupUnlimited = calc.getAppSpeedup(new HashSet<SRegionInfo>(list));
		perfMap.put(-1, speedupUnlimited);
		
		for (int core : perfMap.keySet()) {
			System.out.printf("Core %d\t Speedup %.2f\n", core, perfMap.get(core));
		}
	}
	
	public static void main(String args[]) {
		System.out.println("pyrprof ver 0.1");		
		
		if (args.length < 1) {
			
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb/ft";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/330.art_m";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/320.equake_m";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/loops/dist10";
			//rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-u/is";
			//rawDir = "f:\\Work\\ep";

			//ParameterSet.rawDir = "f:\\Work\\npb-u\\is";		
			//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/npb-b/cg";
			//ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/regression/predictionWork";
			ParameterSet.rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/sd-vbs/sift";
			//String[] splitted = rawDir.split("/");
			//project = splitted[splitted.length-1];			
			ParameterSet.project = "sift";
			ParameterSet.excludeFile = "sift.exclude";
			
		} else {
			ParameterSet.setParameter(args);
		}
		
		String rawDir = ParameterSet.rawDir;
		String project = ParameterSet.project;
		
		
		FilterControl filter = new FilterControl(ParameterSet.minSP, ParameterSet.minSpeedup, 
				ParameterSet.minSpeedupDOACROSS, ParameterSet.outerIncentive);

		System.out.print("\nPlease Wait: Loading Trace Files...");
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";

		long start = System.currentTimeMillis();
		SRegionManager sManager = new SRegionManager(new File(sFile), false);
		URegionManager dManager = new URegionManager(sManager, new File(dFile));
		long end = System.currentTimeMillis();
		OMPGrepReader reader = new OMPGrepReader(rawDir + "/omp.txt");
		SRegionProfileAnalyzer profileAnalyzer = new SRegionProfileAnalyzer(dManager, reader);
		SRegionInfoAnalyzer analyzer = dManager.getSRegionAnalyzer();
		profileAnalyzer.dump(rawDir + "/analysis.txt");
		/*
		profileAnalyzer.dumpGroup(rawDir + "/analysis_grp.txt");
		profileAnalyzer.reportFalsePositiveRegions(rawDir + "/falsePositive.txt");
		profileAnalyzer.reportFalseNegativeRegions(rawDir + "/falseNegative.txt");
		*/
		
		
		String fileName = rawDir + "/" + project + ".dat";
		profileAnalyzer.writeDatFile(fileName);
		
		String relationFile = rawDir + "/" + project + ".rel";
		profileAnalyzer.writeRelFile(relationFile);
		
		//String planFile = rawDir + "/" + project + ".plan";
		//String[] planArgs = {rawDir, project};
		//System.out.println(planArgs[0] + "  " + planArgs[1]);
		//assert(false);
		//PyrProfRunner.main(planArgs);
		/*
		RefPlanner refAnalyzer = new RefPlanner(dManager.getSRegionAnalyzer());
		List<SRegionInfo> refList = refAnalyzer.recommend(ParameterSet.minSpeedup);
		String refFile = rawDir + "/" + project + ".ref";
		refAnalyzer.emitParallelRegions(refFile);
		
		List<SRegionInfo> manualList = refAnalyzer.recommend(0.99);
		String manualFile = rawDir + "/" + project + ".manual";
		refAnalyzer.emitParallelRegions(manualFile);*/
		
		/*
		GprofAnalyzer gprofAnalyzer = new GprofAnalyzer(dManager.getSRegionAnalyzer());	
		PyrComparator comparator = new PyrComparator(refList);
		List<SRegionInfo> gprofList = gprofAnalyzer.recommend((minSpeedup - 1.0) * 100.0);
		//List<SRegionInfo> manualList = dManager.getSRegionAnalyzer().getExploitedList();
		List<SRegionInfo> tempList = dManager.getSRegionAnalyzer().getPyrprofList();
		List<SRegionInfo> pyrprofList = SRegionInfoFilter.filterMinSpeedup(tempList, minSpeedup);		
		List<SRegionInfo> doallList = SRegionInfoFilter.filterNonDoall(analyzer, pyrprofList, minSP);
		List<SRegionInfo> outDoallList = SRegionInfoFilter.filterNonOuterDoall(analyzer, pyrprofList, minSP);
		List<SRegionInfo> pyrplanList = PyrProfRunner.getRecommendedList(dManager, analyzer, minSpeedup, minSP);		
		comparator.evalute(gprofList, "gprof");
		comparator.evalute(manualList, "manual");
		comparator.evalute(pyrprofList, "pyrprof");
		comparator.evalute(doallList, "doall");
		comparator.evalute(outDoallList, "outer doall");
		comparator.evalute(pyrplanList, "pyrplan");
		//PloticusChartGenerator chart = new PloticusChartGenerator(analyzer.analyzer, reader);
		//chart.write(rawDir + "/chart.pl", fileName);
		*/		
		
		String dpPlanFile = rawDir + "/" + project + ".dp";
		String dpPlanRegionFile = rawDir + "/" + project + ".pyrprof";
		
		String fullExcludeFile = rawDir + "/" + project + ".exclude";
		//DPPlanner planner = new DPPlanner(analyzer, fullExcludeFile);		
		//List<SRegionInfo> planInfo = planner.plan(filter);
		
		String parallelFile = rawDir + "/" + project + ".parallel";
		//List<SRegionInfo> planInfo = ParallelFileReader.readParallelFile(parallelFile, analyzer);
		
		//DPPlannerPlus plannerPlus = new DPPlannerPlus(analyzer, fullExcludeFile);		
		//List<SRegionInfo> planPlus = plannerPlus.plan(filter);		
		//plannerPlus.emitParallelRegions(dpPlanRegionFile);
		
		/*
		SRegionSpeedupCalculator calcSP = new SRegionSpeedupCalculator(ScaleMode.LINEAR, LimitFactor.SP, 200.0);
		SRegionSpeedupCalculator calcGranularity = new SRegionSpeedupCalculator(ScaleMode.LINEAR, LimitFactor.GRANULARITY, 200.0);
		SRegionSpeedupCalculator calcBwLow = new SRegionSpeedupCalculator(ScaleMode.LINEAR, LimitFactor.ALL, 100.0);
		SRegionSpeedupCalculator calcBwMed = new SRegionSpeedupCalculator(ScaleMode.LINEAR, LimitFactor.ALL, 200.0);
		SRegionSpeedupCalculator calcBwHi = new SRegionSpeedupCalculator(ScaleMode.LINEAR, LimitFactor.ALL, 300.0);
				
		printEstimatedSpeedups("SP", calcSP, planPlus);
		printEstimatedSpeedups("Granularity", calcGranularity, planPlus);
		printEstimatedSpeedups("BW_Low", calcBwLow, planPlus);
		printEstimatedSpeedups("BW_Med", calcBwMed, planPlus);
		printEstimatedSpeedups("BW_Hi", calcBwHi, planPlus);*/
		//printEstimatedSpeedups("Sqrt", calcSqrt, planPlus);
		/*
		double speedup0 = 1.00 / (1.00 - planner.getTimeSavingPercent() / 100.0);
		double speedup1 = 1.00 / (1.00 - plannerPlus.getTimeSavingPercent(1) / 100.0);
		double speedup2 = 1.00 / (1.00 - plannerPlus.getTimeSavingPercent(2) / 100.0);
		double speedup4 = 1.00 / (1.00 - plannerPlus.getTimeSavingPercent(4) / 100.0);
		double speedup8 = 1.00 / (1.00 - plannerPlus.getTimeSavingPercent(8) / 100.0);
		double speedup16 = 1.00 / (1.00 - plannerPlus.getTimeSavingPercent(16) / 100.0);
		double speedup32 = 1.00 / (1.00 - plannerPlus.getTimeSavingPercent(32) / 100.0);
		double speedupUnlimited = 1.00 / (1.00 - plannerPlus.getTimeSavingPercent() / 100.0);*/
		/*
		System.out.printf("\n[1, 2, 4, 8, 16, 32, unlimited core, SP base]\n%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n", 
				speedup1, speedup2, speedup4, speedup8, speedup16, speedup32, speedupUnlimited, speedup0);*/
		
		
		
		//List<SRegionInfo> planInfo = SRegionInfoFilter.toSRegionInfoList(analyzer, plan);
		//String planFile = rawDir + "/" + project + ".plan";
		//SRegionInfoPrinter.dumpFile(planInfo, planFile);
		/*
		
		
		for (SRegionInfo each : planInfo) { 
			System.out.println(each.getSRegion() + " exploited: " + each.isExploited());
			for (URegion region : each.getInstanceSet()) {
				System.out.println("\t" + region + "\n");
			}
		}*/
		//PyrComparator comparator2 = new PyrComparator(manualList);
		//comparator2.emitAnalysis(planInfo, dpPlanFile);
		
		PerfPredictor predictor = new PerfPredictor(dManager);
		String predictFile = rawDir + "/" + project + ".predict";
		//predictor.predict(planInfo, ParameterSet.minWorkChunk, ParameterSet.bwThreshold, predictFile);
		//predictor.predict(planInfo, 20000.0, 0.32, predictFile);
		//predictor.predict(planInfo, 20000.0, 0.32, predictFile);
		//analyzer.checkUniquification();
	}
}

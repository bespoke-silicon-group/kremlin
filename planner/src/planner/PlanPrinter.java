package planner;

import java.util.ArrayList;
import java.util.List; 
import kremlin.*;

public class PlanPrinter {
	public static void print(CRegionManager manager, Plan plan, double threshold) {
		long serial = (long)plan.getSerialTime();
		System.out.printf("Target : %s\n", plan.getTarget());
		System.out.printf("Speedup: %.2f\n", 100.0 / (100.0 - plan.getTimeReduction()));
		System.out.printf("Serial  : %d\n", serial);
		System.out.printf("Parallel: %d\n\n", (int)(serial * 0.01 * (100.0 - plan.getTimeReduction())));
		
		List<CRegion> list = new ArrayList<CRegion>();
		for (CRegionRecord each : plan.getCRegionList())
			list.add(each.getCRegion());
		
		//CRegionPrinter rPrinter = new CRegionPrinter(manager);
		//rPrinter.printRegionList(list);
		PlanPrinter printer = new PlanPrinter(manager, plan);
		int index = 0;
		for (CRegionRecord each : plan.getCRegionList()) {
			if (manager.getTimeReduction(each.getCRegion()) < threshold)
				break;
			System.out.printf("[%2d] %s\n\n", index++, printer.getCRegionRecordString(each));
			
		}
	}
	
	CRegionManager manager;
	Plan plan;
	KremlinPrinter printer;
	//CRegionPrinter regionPrinter;
	
	//public CRegionPrinter(CRegionManager manager) {
	//	this.manager = manager;
	//}
	
	PlanPrinter(CRegionManager manager, Plan plan) {
		this.manager = manager;
		this.plan = plan;
		this.printer = KremlinPrinter.configure(manager);
		//this.regionPrinter = new CRegionPrinter(manager);
	}
	
	String getCRegionRecordString(CRegionRecord record) {
		CRegion region = record.getCRegion();
		double timeSave = record.getTimeSave();
		
		/*
		String context = regionPrinter.getContextString(region);
		String stat0 = regionPrinter.getStatString(region);
		String stat1 = regionPrinter.getStatString2(region);
		String ret = String.format("ExecTimeReduction: %.2f %% %s\n%s\n%s", timeSave, stat0, stat1, context);*/		
		//String ret = String.format("ExecTimeReduction: %.2f %% %s\n%s", timeSave, region.getStatString(),  context);
		//String ret = String.format("%s\n%s\n%s", region.toString(), region.getStatString(), context);
		String first = null;
		if (KremlinConfig.isVerbose()) {
			first = String.format("TimeRed(%d)=%.2f%%, %s", 
					record.getCoreCount(), timeSave, printer.getVerboseString(region));
			
		} else {			
			first = String.format("TimeRed(%d)=%.2f%%, %s", 
					record.getCoreCount(), timeSave, printer.getSummaryString(region));
		}
				
		String context = printer.getContextString(region);
		return first + "\n" + context;
	}
	
	
	
	
	
	
	
	
	
	
}

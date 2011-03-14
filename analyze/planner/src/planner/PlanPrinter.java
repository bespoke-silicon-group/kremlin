package planner;

import java.util.ArrayList;
import java.util.List;

import pprof.*;

public class PlanPrinter {
	public static void print(CRegionManager manager, Plan plan) {
		long serial = (long)plan.getSerialTime();
		System.out.printf("Target : %s\n", plan.getTarget());
		System.out.printf("Speedup: %.2f\n", 100.0 / (100.0 - plan.getTimeReduction()));
		System.out.printf("Serial  : %d\n", serial);
		System.out.printf("Parallel: %d\n\n", (int)(serial * 0.01 * (100.0 - plan.getTimeReduction())));
		
		List<CRegion> list = new ArrayList<CRegion>();
		for (CRegionRecord each : plan.getCRegionList())
			list.add(each.getCRegion());
		
		CRegionPrinter rPrinter = new CRegionPrinter(manager);
		//rPrinter.printRegionList(list);
		PlanPrinter printer = new PlanPrinter(manager, plan);
		int index = 0;
		for (CRegionRecord each : plan.getCRegionList()) {			
			System.out.printf("[%2d] %s\n", index++, printer.getCRegionRecordString(each));
		}
	}
	
	CRegionManager manager;
	Plan plan;
	CRegionPrinter regionPrinter;
	
	PlanPrinter(CRegionManager manager, Plan plan) {
		this.manager = manager;
		this.plan = plan;
		this.regionPrinter = new CRegionPrinter(manager);
	}
	
	String getCRegionRecordString(CRegionRecord record) {
		CRegion region = record.getCRegion();
		double timeSave = record.getTimeSave();
		String context = regionPrinter.getContextString(region);
		String stat0 = regionPrinter.getStatString(region);
		String stat1 = regionPrinter.getStatString2(region);
		String ret = String.format("ExecTimeReduction: %.2f %%\n%s\n%s\n%s", timeSave, stat0, stat1, context);		
		return ret;
	}
}

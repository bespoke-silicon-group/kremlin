package kremlin;

import java.util.ArrayList;
import java.util.List;

public class KremlinPrinter {
	static KremlinPrinter instance;

	CRegionManager manager;
	
	static public KremlinPrinter configure(CRegionManager manager) {		
		if (KremlinPrinter.instance == null) {
			KremlinPrinter.instance = new KremlinPrinter(manager);			
		}
		return KremlinPrinter.instance;
	}
	
	
	private KremlinPrinter(CRegionManager manager) {
		this.manager = manager;
	}
	
	/*
	 * Gets string with info about the context in which a given CRegion
	 * occurred (i.e.  where it was called from.
	 */
	String contextFuncString(CRegion region) {
		SRegion sregion = region.getSRegion();
		assert(sregion.getType() == RegionType.FUNC);
		CallSite callsite = region.getCallsite();
		String module = "root";
		int startLine = 0;
		
		if (callsite != null) {
			module = callsite.getModule();
			startLine = callsite.getStartLine();			
		}
		return String.format("%s called at file %10s, line %4d", sregion, module, startLine); 
	}
	
	/*
	 * Gets a list of strings representing the contexts for all ancestors of
	 * the given CRegion that are functions (i.e. have a callsite).
	 */
	public List<String> getContextStringList(CRegion region) {
		List<String> ret = new ArrayList<String>();		
		
		String targetString = region.getSRegion().toString();
		if (region.getSRegion().getType() == RegionType.FUNC)
			targetString = contextFuncString(region);
		ret.add(targetString);		
		
		CRegion current = region.getParent();
		while (current != null  && current != manager.getRoot()) {
			SRegion sregion = current.getSRegion();
			if (sregion.getType() == RegionType.FUNC && current.getCallsite() != null) {
				ret.add(contextFuncString(current));				
			}
			current = current.getParent();
		}
		return ret;
	}
	
	/*
	 * Converts list of strings to a single string.
	 */
	String fromListToString(List<String> list) {
		StringBuffer buf = new StringBuffer();
		for (int i=0; i<list.size(); i++) {
			String each = list.get(i);
			buf.append(each);
			if (i != list.size() - 1)
				buf.append("\n");
		}
		
			
		return buf.toString();
	}
	
	/* 
	 * Gets a string representing callsite info for all ancestors of the given
	 * CRegion that are functions.
	 */
	public String getContextString(CRegion region) {				
		List<String> list = getContextStringList(region);
		return fromListToString(list);		
	}
	
	/*
	 * Produce a string with some basic status about a given CRegion.
	 */
	public String getStatString(CRegion region) {		
		assert(region != null);
		CRecursiveType recursiveType = region.getRecursiveType();
		assert(recursiveType != null);
		double timeReduction = manager.getTimeReduction(region);
		
		String stats = String.format(
				"ID = %d, IdealTimeReduction = %5.2f%% , type = [%s, %s], cov = %5.2f%%",
				region.getId(), timeReduction, region.getParallelismType(), 
				recursiveType.toString(), manager.getCoverage(region));
		return stats;
	}	

	/*
	 * Produces a string with some "additional" stats about a given CRegion.
	 */
	public String getStatString2(CRegion region) {
		 String stats = String.format("count = %5d, sp = %5.2f [%5.2f - %5.2f]",
				 region.getInstanceCount(), region.getSelfP(), region.getMinSelfP(), region.getMaxSelfP());	                
		 return stats;
	}

	/*
	 * Gets a string representing the given CRegion.
	 * This string will contain both the context (i.e. call strack trace) and
	 * basic stats about the region.
	 */
	public String getString(CRegion region) {		
		String context = getContextString(region);
		String stats = getStatString(region);
		String ret = String.format("%s\t\t%s", context, stats);
		return ret;		
	}
	
	/*
	 * Gets a "summary" of given CRegion, including the ideal time reduction,
	 * coverage, and self-parallelism.
	 */
	public String getSummaryString(CRegion region) {		
		String ret = String.format("TimeRed(Ideal)=%.2f%%, Cov=%.2f%%, SelfP=%.2f, %s", 
			manager.getTimeReduction(region), manager.getCoverage(region), region.getSelfP(), region.getParallelismType());				
		
		return ret;
	}	
	
	/*
	 * Gets string with self-parallelism stats for the given CRegion.
	 * This info will include average, min, and max SP.
	 */
	String getSelfParallelismString(CRegion region) {
		String ret = String.format("SelfP=%.2f [%5.2f - %5.2f]",
			region.getSelfP(), region.getMinSelfP(), region.getMaxSelfP());		
		return ret;
	}

	/*
	 * Gets string with total-parallelism stats for the given CRegion.
	 */
	String getTotalParallelismString(CRegion region) {
		String ret = String.format("TotalP=%.2f",
			region.getTotalParallelism());
		return ret;
	}
	
	/*
	 * Gets string with instance count stats for the given CRegion.
	 * This info will include average, min, and max num of instances.
	 */
	String getIterString(CRegion region) {
		String ret = "";
		if (region.getSRegion().getType() == RegionType.LOOP) {
			CRegionStat stat = region.getRegionStat();
			String iter = String.format("Iter=%.2f [%d - %d]",
					stat.getAvgIter(), stat.getMinIter(), stat.getMaxIter());
			ret = ret + iter;			
		}
		return ret;
	}
	
	/*
	 * Gets string with "relationship" info for the given CRegion.
	 * This info will include the type of the region and number of instances
	 * as well as the IDs of it and its parent.
	 */
	String getRelationString(CRegion region) {
		long parentId = 0;
		if (region.getParent() != null) {
			parentId = region.getParent().getId();
		}
		String ret = String.format("Type=%s, ID(self)=%d, ID(parent)=%d, nInstance=%d, nChildren=%d",
			region.getRecursiveType(), region.getId(), parentId, region.getInstanceCount(), region.getChildrenSet().size());
		return ret;
	}
	
	/*
	 * Produce a list of strings that contains summary, parallelism, instance
	 * count, and relationship strings.
	 */
	public List<String> getVerboseStringList(CRegion region) {
		List<String> ret = new ArrayList<String>();
		ret.add(getSummaryString(region));
		ret.add(String.format("%s, %s, %s", getTotalParallelismString(region), getSelfParallelismString(region), getIterString(region))); 
		ret.add(getRelationString(region));
		return ret;		
	}
	
	/*
	 * Gets a string with "full" info about a region, including it's
	 * relationships, self-parallelism, and number of instances.
	 */
	public String getVerboseString(CRegion region) {
		List<String> list = getVerboseStringList(region);
		return fromListToString(list);
	}
	
	/*
	 * XXX: Unused. What was this for?
	 * Gets list of strings corresponding to recursive levels for a given
	 * CRegion.
	 * Each string will contain stat info for each recursive level.
	 */
	public List<String> getRecursiveStringList(CRegion region) {
		List<String> ret = new ArrayList<String>();
		if (region.getRecursiveType() == CRecursiveType.NORMAL) 
			return ret;
		
		CRegionR rRegion = (CRegionR)region;
		for (int i=0; i<rRegion.getRecursionDepth(); i++) {
			CRegionStat stat = rRegion.getStat(i);
			ret.add(stat.toString());
		}		 
		
		return ret;
	}
	
	public String getRecursiveString(CRegion region) {
		return this.fromListToString(getRecursiveStringList(region));
	}
}

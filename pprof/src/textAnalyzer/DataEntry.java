package textAnalyzer;
import pprof.*;

public class DataEntry {
	//regionCode, tp, sp, coverage, speedup, parallelized, ancestor, type, nsp,
	//sregion.getFuncName(), sregion.getStartLine(), sregion.getEndLine(), isLeaf, groupParallelized
	RegionType type;
	double tp, sp, coverage, speedup;
	boolean parallelized;
	boolean ancestorParallelized;
	int typeNum; //ignore this
	double nsp;
	String funcName;
	int startLine, endLine;
	boolean isLeaf;
	boolean isGroupParallelized;	
	
	DataEntry(String input) {
		String[] splitted = input.split("\t");
		this.type =  getRegionType(splitted[0]);
		this.tp = Double.parseDouble(splitted[1]);
		this.sp = Double.parseDouble(splitted[2]);
		this.coverage = Double.parseDouble(splitted[3]);
		this.speedup = Double.parseDouble(splitted[4]);
		this.parallelized = getBooleanFromInt(splitted[5]);
		this.ancestorParallelized = getBooleanFromInt(splitted[6]);
		this.typeNum = Integer.parseInt(splitted[7]);
		this.nsp = Double.parseDouble(splitted[8]);
		this.funcName = splitted[9];
		this.startLine = Integer.parseInt(splitted[10]);
		this.endLine = Integer.parseInt(splitted[11]);
		this.isLeaf = getBooleanFromInt(splitted[12]);
		this.isGroupParallelized = getBooleanFromInt(splitted[13]);		
	}
	
	RegionType getRegionType(String string) {
		int code = Integer.parseInt(string);
		if (code == 0)
			return RegionType.FUNC;
		else if (code == 1)
			return RegionType.LOOP;
		else if (code == 2)
			return RegionType.BODY;
		else {
			assert(false);
			return null;
		}			
	}
	
	boolean getBooleanFromInt(String string) {
		if (string.equals("1"))
			return true;
		else if (string.equals("0"))
			return false;
		else {
			assert(false);
			return false;
		}
			
	}
	
	public String toString() {
		return String.format("%s %s %d %d", type, funcName, startLine, endLine);
	}
}

package visualizer;

public class CorrelationEntry {
	int type;
	double totalP;
	double selfP;
	double coverage;
	int isParallelized;
	int isAncestorParallelized;
	int code;
	CorrelationEntry(String line) {
		String[] splitted = line.split("\t");
		type = Integer.parseInt(splitted[0]);
		totalP = Double.parseDouble(splitted[1]);
		selfP = Double.parseDouble(splitted[2]);
		coverage = Double.parseDouble(splitted[3]);
		isParallelized = Integer.parseInt(splitted[4]);
		isAncestorParallelized = Integer.parseInt(splitted[5]);
		code = Integer.parseInt(splitted[6]);
	}
	
	public String toString() {
		String ret = String.format("%d\t%.2f\t%.2f\t%.2f\t%d\t%d\t%d", 
				type, totalP, selfP, coverage, isParallelized, isAncestorParallelized, code);
		return ret;
	}
}

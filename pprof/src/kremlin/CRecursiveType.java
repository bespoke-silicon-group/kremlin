package pprof;

public enum CRecursiveType {
	NORMAL, 
	REC_INIT, 
	REC_SINK,
	REC_NORM;
	
	public String toString() {
		if (this == NORMAL)
			return "Norm";
		else if (this == REC_INIT)
			return "RInit";
		else if (this == REC_SINK)
			return "RSink";
		else if (this == REC_NORM)
			return "RNorm";
		else
			return "ERR";
	}
};
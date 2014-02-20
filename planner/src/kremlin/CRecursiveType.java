package kremlin;

/*
 * Enum to represent types of CRegion.
 * Options include: normal (not recursive), rec_init (init node), rec_sink
 * (sink node), rec_norm (XXX).
 */
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

package pprof;

public enum PType {
	DOALL, 
	DOACROSS, 
	TLP, 
	ILP;
	
	public String toString() {
		if (this == DOALL)
			return "DOALL";
		else if (this == DOACROSS)
			return "DOACROSS";
		else if (this == TLP)
			return "TLP";
		else
			return "ILP";
	}
}

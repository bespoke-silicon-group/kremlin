package kremlin;

/*
 * Enum to represent the various possible types of parallelism in a program
 * region.
 * Options include: DOALL, DOACROSS, thread-level parallelism (TLP), and
 * instruction-level parallelism (ILP).
 */
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

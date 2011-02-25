package planner;

public class Target {
	int overhead;
	int numCore;
	
	public Target(int numCore, int overhead) {
		this.numCore = numCore;
		this.overhead = overhead;
	}
	
	public String toString() {
		return String.format("NumCore = %d, Overhead = %d", numCore, overhead);
	}
	
	
}

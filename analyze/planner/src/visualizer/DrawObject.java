package visualizer;

public class DrawObject implements Comparable {
	DrawObject(long parallelism, long work) {
		this.parallelism = parallelism;
		this.work = work;
	}

	long parallelism;
	long work;
	
	long getGain() {
		return this.work - this.work / this.parallelism;
	}
	
	public int compareTo(Object o) {
		DrawObject target = (DrawObject)o;
		//return (int)(target.getGain() - this.getGain());
		return (int)(target.parallelism - this.parallelism);
	}
}
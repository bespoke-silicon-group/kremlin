package visualizer;

import java.util.ArrayList;


public class DrawSet {
	
	
	DrawObject parent;
	java.util.List<DrawObject> children;

	public DrawSet(long parallelism, long work) {
		parent = new DrawObject(parallelism, work);
		children = new ArrayList<DrawObject>();
	}
	public void setParent(long parallelism, long work) {
		parent.parallelism = parallelism;
		parent.work = work;
	}
	
	public void addChild(long parallelism, long work) {
		children.add(new DrawObject(parallelism, work));
	}
}

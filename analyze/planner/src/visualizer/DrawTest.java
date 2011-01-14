package visualizer;
import java.awt.*;
import java.awt.event.*;
import java.awt.geom.*;

import java.util.*;


public class DrawTest extends Frame {
	DrawTest() {
		drawSet = new DrawSet(1000, 10000);		
		drawSet.addChild(8, 4000);
		drawSet.addChild(4000, 4000);
		drawSet.addChild(100, 1000);
		generateRectangles(100, 100, 500, 500);
	}
	
	long getMaxParallelism() {
		long max = 0;
		if (drawSet.parent.work > max)
			max = drawSet.parent.parallelism;
		
		
		for (DrawObject each : drawSet.children) {
			if (each.parallelism > max)
				max = each.parallelism;
		}
		return max;
	}
	
	void generateRectangles(long baseX, long baseY, long sizeX, long sizeY) {		
		//int maxHeight = 200;
		
		long startX = baseX;
		long startY = baseY + sizeY;
		long totalWork = drawSet.parent.work;
		long maxParallelism = getMaxParallelism();
		// log2(maxParallelism) : sizeY = log2(parallelism) : height
		
		
		Collections.sort(drawSet.children);
		for (DrawObject each : drawSet.children) {
			long height = (long)(sizeY * Math.log(each.parallelism) 
						/ Math.log(maxParallelism));
			long width = (long)(((double)each.work / totalWork) * sizeX);
			startY = baseY + sizeY - height;
			
			list.add(new Rectangle2D.Double(startX, startY, width, height));
			System.out.printf("%d %d %d %d\n", startX, startY, width, height);
			startX += width;
		}
		
		long height = (long)(sizeY * Math.log(drawSet.parent.parallelism) 
				/ Math.log(maxParallelism));
		long width = sizeX;
		startX = baseX;
		startY = baseY + sizeY - height;
		
		parentRect = new Rectangle2D.Double(startX, startY, width, height);
		
		
		//list.add(new Rectangle2D.Double(baseX + 100, baseY + 100, sizeX/2, sizeY/2));		
		
	}
	DrawSet drawSet;
	Stroke drawingStroke = new BasicStroke(3);
	//Rectangle2D rect = new Rectangle2D.Double(60, 70, 120, 80);
	//Rectangle2D rect2 = new Rectangle2D.Double(160, 170, 120, 80);
	java.util.List<Rectangle2D> list = new ArrayList<Rectangle2D>();
	Rectangle2D parentRect;
	
	private void drawGrid(Graphics g, int gridSpace) {
	      Insets insets = getInsets();
	      int firstX = insets.left;
	      int firstY = insets.top;
	      int lastX = getWidth() - insets.right;
	      int lastY = getHeight() - insets.bottom;

	      //Draw vertical lines.
	      int x = firstX;
	      while (x < lastX) {
	        g.drawLine(x, firstY, x, lastY);
	        x += gridSpace;
	      }

	      //Draw horizontal lines.
	      int y = firstY;
	      while (y < lastY) {
	    	g.setColor(Color.gray);	    	  
	        g.drawLine(firstX, y, lastX, y);
	        y += gridSpace;
	      }
	    }

	public void paint(Graphics g) {
		Graphics2D g1 = (Graphics2D) g;
		g1.setStroke(drawingStroke);
		
		//drawGrid(g, 100);
		
		for (Rectangle2D each : list) {
			g1.setColor(Color.BLACK);
			g1.draw(each);
		}
		
		g1.setColor(Color.RED);
		g1.draw(parentRect);
		/*
		g1.draw(rect);
		g1.draw(rect2);
		g1.setPaint(Color.yellow);
		g1.fill(rect);
		g1.setPaint(Color.RED);
		g1.fill(rect2);*/
	}

	public static void main(String args[]) {

		Frame frame = new DrawTest();
		frame.addWindowListener(new WindowAdapter() {
			public void windowClosing(WindowEvent we) {
				System.exit(0);
			}
		});
		frame.setSize(300, 200);
		frame.setVisible(true);
	}
}
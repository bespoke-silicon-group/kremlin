package visualizer;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.GradientPaint;
import java.util.*;

import org.jfree.chart.ChartFactory;
import org.jfree.chart.ChartPanel;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.axis.*;
import org.jfree.chart.plot.*;
import org.jfree.chart.renderer.AreaRendererEndType;
import org.jfree.chart.renderer.category.AreaRenderer;
import org.jfree.chart.renderer.xy.*;
import org.jfree.data.category.CategoryDataset;
import org.jfree.data.category.DefaultCategoryDataset;
import org.jfree.ui.ApplicationFrame;
import org.jfree.ui.RefineryUtilities;
import org.jfree.data.xy.*;

public class ChartGenerator extends ApplicationFrame {
	ChartGenerator(String title) {
		super(title);
		
		DrawSet drawSet = new DrawSet(1000, 100);		
		drawSet.addChild(8, 20);
		drawSet.addChild(4000, 40);
		drawSet.addChild(100, 20);
		
		XYDataset dataset = createDataset(drawSet);
        JFreeChart chart = createChart(dataset);
        ChartPanel chartPanel = new ChartPanel(chart);
        chartPanel.setFillZoomRectangle(true);
        chartPanel.setMouseWheelEnabled(true);
        chartPanel.setPreferredSize(new Dimension(500, 270));
        setContentPane(chartPanel);
	}
	
	public static ChartPanel getChart(DrawSet input) {
		XYDataset dataset = createDataset(input);
        JFreeChart chart = createChart(dataset);
        ChartPanel chartPanel = new ChartPanel(chart);
        return chartPanel;
	}
	/**
     * Returns a sample dataset.
     *
     * @return The dataset.
     */
    private static XYDataset createDataset(DrawSet input) {
        // create the dataset...
        //DefaultCategoryDataset dataset = new DefaultCategoryDataset();
        DefaultTableXYDataset dataset = new DefaultTableXYDataset();
        //java.util.List<XYSeries> list = new ArrayList<XYSeries>();
        
        int index = 0;
        int startX = 0;
        
        //List<DrawObject> list = new ArrayList<DrawObject>(input.children);
        //list.add(input.parent);
        
        for (DrawObject each : input.children) {
        	XYSeries series = new XYSeries(index++, false, false);
        	if (startX != 0)
        		series.add(0, 1);
        	series.add(startX - 0.01, 1);
        	series.add(startX, each.parallelism);
        	if (each.work != 0)
        		series.add(startX + each.work, each.parallelism);
        	series.add(startX + each.work + 0.01, 1);
        	startX += each.work;
        	//list.add(series);
        	dataset.addSeries(series);
        }
        
        
        XYSeries series = new XYSeries(index++, false, false);
        series.add(0, input.parent.parallelism);
        series.add(100, input.parent.parallelism);
        
        dataset.addSeries(series);
        
        System.out.println("Drawing " + input.children.size() + " series");
               
        return dataset;

    }

    /**
     * Creates a sample chart.
     *
     * @param dataset  the dataset.
     *
     * @return The chart.
     */
    private static JFreeChart createChart(XYDataset dataset) {

        // create the chart...
    	JFreeChart chart = ChartFactory.createXYAreaChart(
    			"pyrChart", 
    			"Work", 
    			"Parallelism", 
    			dataset, 
    			PlotOrientation.VERTICAL, 
    			true, 
    			true,
    			false);
    	/*
    	JFreeChart chart = ChartFactory.createAreaChart(
    			"Demo", 
    			"Category", 
    			"Value", 
    			dataset, 
    			PlotOrientation.VERTICAL, 
    			true, 
    			true, 
    			false);*/
    	/*
        JFreeChart chart = ChartFactory.createBarChart(
            "Bar Chart Demo 1",       // chart title
            "Category",               // domain axis label
            "Value",                  // range axis label
            dataset,                  // data
            PlotOrientation.VERTICAL, // orientation
            true,                     // include legend
            true,                     // tooltips?
            false                     // URLs?
        );*/

        // NOW DO SOME OPTIONAL CUSTOMISATION OF THE CHART...

        // set the background color for the chart...
        chart.setBackgroundPaint(Color.white);
        

        // get a reference to the plot for further customisation...
        XYPlot plot = (XYPlot) chart.getPlot();
        
        //final NumberAxis rangeAxis = new LogarithmicAxis("Parallelism");        
        //plot.setRangeAxis(rangeAxis);
        //plot.drawHorizontalLine()
        
        //plot.setForegroundAlpha(NORMAL);

        // ******************************************************************
        //  More than 150 demo applications are included with the JFreeChart
        //  Developer Guide...for more information, see:
        //
        //  >   http://www.object-refinery.com/jfreechart/guide.html
        //
        // ******************************************************************

        // set the range axis to display integers only...
        //NumberAxis rangeAxis = (NumberAxis) plot.getRangeAxis();
        //rangeAxis.setStandardTickUnits(NumberAxis.createIntegerTickUnits());
        

        // disable bar outlines...
        XYAreaRenderer renderer = (XYAreaRenderer) plot.getRenderer();
        //renderer.drawRangeMarker(arg0, arg1, arg2, arg3, arg4)
        
        //renderer.s
        //AreaRenderer renderer = (AreaRenderer) plot.getRenderer();
        //renderer.setEndType(AreaRendererEndType.TRUNCATE);
        //renderer.setDrawBarOutline(false);

        // set up gradient paints for series...
        /*
        GradientPaint gp0 = new GradientPaint(0.0f, 0.0f, Color.blue,
                0.0f, 0.0f, new Color(0, 0, 64));
        GradientPaint gp1 = new GradientPaint(0.0f, 0.0f, Color.green,
                0.0f, 0.0f, new Color(0, 64, 0));
        GradientPaint gp2 = new GradientPaint(0.0f, 0.0f, Color.red,
                0.0f, 0.0f, new Color(64, 0, 0));
        renderer.setSeriesPaint(0, gp0);
        renderer.setSeriesPaint(1, gp1);
        renderer.setSeriesPaint(2, gp2);*/

        /*
        XYAxis domainAxis = plot.getDomainAxis();
        domainAxis.setCategoryLabelPositions(
                CategoryLabelPositions.createUpRotationLabelPositions(
                        Math.PI / 6.0));*/
        // OPTIONAL CUSTOMISATION COMPLETED.

        return chart;

    }
    
	public static void main(String[] args) {
        ChartGenerator demo = new ChartGenerator("Bar Chart Demo 1");
        demo.pack();
        RefineryUtilities.centerFrameOnScreen(demo);
        demo.setVisible(true);
    }
}

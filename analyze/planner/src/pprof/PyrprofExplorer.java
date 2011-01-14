package pprof;

import javax.swing.*;
import javax.swing.event.*;
import javax.swing.tree.*;

import pyrplan.Recommender;

import java.awt.event.*;
import visualizer.*;


import java.net.*;
import java.awt.*;
import java.io.*;

import java.util.*;

public class PyrprofExplorer extends JPanel
							implements 
							TreeSelectionListener, 
							ActionListener {

	private JEditorPane htmlPane;	
	private URL helpURL;
	private static boolean DEBUG = false;
	
	private static boolean playWithLineStyle = false;
	private static String lineStyle = "Horizontal";
	private static boolean useSystemLookAndFeel = true;
	
	JTree tree;
	
	JSplitPane infoPanel;
	URegionManager manager;
	FreqAnalyzer freq;
	SelfParallelismAnalyzer rp;
	SRegionInfoAnalyzer analyzer;
	DrawSetGenerator generator;
	ButtonPanel buttonPanel;
	JSplitPane splitPaneLeaf;	
	SRegionManager sManager;	
	Recommender recommender;
	DefaultMutableTreeNode rootNode;
	
	JComponent topPanel;
	JComponent midPanel;
	JComponent bottomPanel;
	
	public PyrprofExplorer() {
		super(new GridLayout(0, 1));
		/*
		this.manager = manager;
		this.freq = new FreqAnalyzer(manager);
		this.rp = new SelfParallelismAnalyzer(manager);
		this.analyzer = manager.getSRegionAnalyzer();
		this.generator = new DrawSetGenerator(analyzer);*/
		//this.fc = new JFileChooser();
								
		
		
		this.topPanel = createTopPanel();
		this.midPanel = createInfoPanel();
		this.bottomPanel = createButtonPanel(); 
		
		Dimension minimumSize = new Dimension(100, 50);		
		//treeView.setMinimumSize(minimumSize);
		//splitPane.setDividerLocation(100);
		//splitPane.setPreferredSize(new Dimension(500, 300));
		
		
		add(topPanel);
		add(midPanel);
		add(bottomPanel);
	}
	
	JScrollPane createTreePanel() {
		DefaultMutableTreeNode top = 
			new DefaultMutableTreeNode("Recommended Regions");
		this.rootNode = top;
		if (this.manager != null)
			createNodes(top);
		
		this.tree = new JTree(top);
		//tree = new JTree(list);
		tree.getSelectionModel().setSelectionMode
			(TreeSelectionModel.SINGLE_TREE_SELECTION);
		
		tree.addTreeSelectionListener(this);
		
		if (playWithLineStyle) {
			tree.putClientProperty("JTree.lineStyle", lineStyle);
		}
		
		JScrollPane treeView = new JScrollPane(tree);
		return treeView;
	}
	
	JList createExcludeList() {
		JList ret = new JList();
		return ret;
	}
	
	JList createParallelizedList() {
		JList ret = new JList();
		return ret;
	}
	
	JComponent createTopPanel() {
		JSplitPane ret = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT);
		ret.setLeftComponent(createTreePanel());
		
		//createTreePanel()
		
		JSplitPane statusPanel = new JSplitPane(JSplitPane.VERTICAL_SPLIT);
		statusPanel.setTopComponent(new JScrollPane(createExcludeList()));
		statusPanel.setBottomComponent(new JScrollPane(createParallelizedList()));
		ret.setRightComponent(statusPanel);	
		return ret;
	}
	
	JPanel createButtonPanel() {
		/*
		JPanel buttonPane = new JPanel();
		//buttonPane.setEditable(false);
		this.loadSButton = new JButton("Set SFile");
		this.loadDButton = new JButton("Set DFile");
		JButton b2 = new JButton("Recommend");
		buttonPane.add(loadSButton);
		buttonPane.add(loadDButton);
		buttonPane.add(b2);
		
		loadSButton.addActionListener(this);
		loadDButton.addActionListener(this);
		b2.addActionListener(this);
		return buttonPane;*/
		this.buttonPanel = new ButtonPanel(this); 
		return this.buttonPanel;
	}
	
	JSplitPane createInfoPanel() {
		htmlPane = new JEditorPane();
		htmlPane.setEditable(false);		
		JPanel chartPanel = new JPanel();
		
		this.infoPanel = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT);
		this.infoPanel.setTopComponent(htmlPane);
		this.infoPanel.setBottomComponent(chartPanel);
		return this.infoPanel;
	}
	
	
	
	public void valueChanged(TreeSelectionEvent e) {
		DefaultMutableTreeNode node = (DefaultMutableTreeNode)
							tree.getLastSelectedPathComponent();
		
		if (node == null)
			return;
		
		TreeEntry nodeInfo = (TreeEntry)node.getUserObject();
		display(nodeInfo.entry);		
		DrawSet set = generator.getDrawSet(nodeInfo.entry.region);
		((JSplitPane)this.midPanel).setBottomComponent(ChartGenerator.getChart(set));
	}
	
	public void actionPerformed(ActionEvent e) {
		System.out.println(e);
		/*
		 if (e.getSource() == loadSButton) {
	            int returnVal = fc.showOpenDialog(this);
	            if (returnVal == JFileChooser.APPROVE_OPTION) {
	                File file = fc.getSelectedFile();
	                this.sFile = file.getAbsolutePath();
	            }
		 }
		 
		 if (e.getSource() == loadDButton) {
			int returnVal = fc.showOpenDialog(this);
            if (returnVal == JFileChooser.APPROVE_OPTION) {
                File file = fc.getSelectedFile();
                this.dFile = file.getAbsolutePath();
            }	            
		 }*/
		
		 if (e.getSource() == buttonPanel.loadButton) {
			String sName = buttonPanel.getSFileName();
			String dName = buttonPanel.getDFileName();
			load(sName, dName);
		 }
		 
		 if (e.getSource() == buttonPanel.recommendButton) {
			 ((JSplitPane)this.topPanel).setLeftComponent(createTreePanel());
			 //this.createNodes(this.rootNode);
			 //this.repaint();
		 }
	}
	
	private void load(String sName, String dName) {
		this.sManager = new SRegionManager(new File(sName), false);
		this.manager = new URegionManager(sManager, new File(dName));
		this.freq = new FreqAnalyzer(manager);
		this.rp = new SelfParallelismAnalyzer(manager);
		this.analyzer = manager.getSRegionAnalyzer();
		this.generator = new DrawSetGenerator(analyzer);
	}	
	
	private void display(SRegionInfo info) {		
		htmlPane.setText(info.toString());		
	}
	
	String getDEntryString(URegion entry) {
		String ret = String.format("sid %d (%s, [%d-%d]) work %d cp %d",				
				entry.sregion.id,
				entry.sregion.func, entry.sregion.startLine, entry.sregion.endLine,
				entry.work, entry.cp);
		return ret;
	}
	
	public String toStringDetail(URegion entry) {
		StringBuffer ret = new StringBuffer("");
		ret.append(String.format("Static Region: %s\n", entry.sregion));
		double parallelism = entry.work / (double)entry.cp;
		ret.append(String.format("CP Length: %d, Parallelism: %.2f, Relative Parallelism: %.2f\n", 
				entry.cp, parallelism, rp.getRP(entry)));
		ret.append(String.format("Work: %d, Instance Count: %d, Total Work: %.2f%%\n", 
				entry.work, freq.getFreq(entry), freq.getCoveragePercentage(entry)));
		ret.append(String.format("\nURegions:\n"));
		
		for (URegion child : entry.children.keySet()) {
			long cnt = entry.getChildCount(child);
			ret.append(String.format("\t[%d] : %s\n", cnt, child.sregion));
		}
		return ret.toString();
	}
	
	class TreeEntry {
		//long num;
		SRegionInfo entry;
		TreeEntry(SRegionInfo entry) {
			this.entry = entry;
			//this.num = num;
		}
		
		public String toString() {
			return String.format("%s() [%d-%d], %s.c", entry.region.func, entry.region.startLine, entry.region.endLine, entry.region.module);
		}
	}
	
	DefaultMutableTreeNode createTreeNode(SRegion region) {
		return new DefaultMutableTreeNode(new TreeEntry(analyzer.getSRegionInfo(region))); 
	}
	
	private DefaultMutableTreeNode createNodeTree(SRegion top) {
		DefaultMutableTreeNode ret = createTreeNode(top);		
		java.util.List<DefaultMutableTreeNode> list = new ArrayList<DefaultMutableTreeNode>();
		list.add(ret);
		
		while (list.isEmpty() == false) {
			DefaultMutableTreeNode removed = list.remove(0);			
			TreeEntry entry = (TreeEntry)removed.getUserObject();
			SRegionInfo info = analyzer.getSRegionInfo(entry.entry.region);

			Set<SRegion> children = info.children;
			for (SRegion child : children) {
				//assert(false);
				DefaultMutableTreeNode toAdd = createTreeNode(child);
				removed.add(toAdd);
				list.add(toAdd);
			}
		}
		
		return ret;
	}
	
	private void createNodes(DefaultMutableTreeNode top) {		
		/*
		Recommender recommender = new Recommender(manager.getSRegionManager(), manager);
		RecList list = recommender.recommend(0.05, new HashSet<SRegion>());
	
		for (int i=list.size()-1; i>=0; i--) {
			RecList.RecUnit unit = list.get(i);
			SRegion region = unit.region;
			DefaultMutableTreeNode toAdd = createNodeTree(region);
			System.out.printf("node: %s, # of children: %d\n", region, toAdd.getChildCount());
			top.add(toAdd);			
		}*/
		java.util.List<SRegionInfo> list = new ArrayList<SRegionInfo>();
		for (SRegion region : sManager.getSRegionSet()) {
			SRegionInfo info = analyzer.getSRegionInfo(region);
			if (info != null)
				list.add(info);
		}
		
		Collections.sort(list);
		
		for (SRegionInfo each : list) {
			DefaultMutableTreeNode toAdd = createNodeTree(each.region);
			//System.out.printf("node: %s, # of children: %d\n", region, toAdd.getChildCount());
			top.add(toAdd);
		}
	}
	
	private static void createAndShowGUI() {
		
		//String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/mpeg_enc";
		//String rawDir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/ft";
		String rawDir = "";
		
		//String rawDir = "c:\\";
		System.out.print("\nPlease Wait: Loading Trace Files...");
		/*
		long start = System.currentTimeMillis();
		String sFile = rawDir + "/sregions.txt";
		String dFile = rawDir + "/cpURegion.bin";		
		SRegionManager sManager = new SRegionManager(new File(sFile));
		URegionManager dManager = new URegionManager(sManager, new File(dFile));		
		long end = System.currentTimeMillis();
		*/		
		
		if (useSystemLookAndFeel) {
			try {
				UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
			} catch (Exception e) {
				System.err.println("Couldn't use system look and feel");
			}
		}
		
		JFrame frame = new JFrame("pyrprof Explorer");
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		
		//frame.add(new pyrprofExplorer(dManager));
		frame.add(new PyrprofExplorer());
		frame.pack();
		frame.setVisible(true);
	}
	
	public static void main(String[] args) {		
		
		javax.swing.SwingUtilities.invokeLater(new Runnable() {
			public void run() {				
				createAndShowGUI();
			}
		});
	}
	
}



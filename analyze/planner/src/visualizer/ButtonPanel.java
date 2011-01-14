package visualizer;

import javax.swing.*;

import pprof.SRegionManager;
import pprof.*;

import java.awt.GridLayout;
import java.awt.event.*;
import java.io.File;
import java.util.*;

public class ButtonPanel extends JPanel implements ActionListener{

	JButton loadSButton;
	JButton loadDButton;
	JFileChooser fc;
	
	//String sFile = "f:\\sqcif\\cg\\sregions.txt";
	//String dFile = "f:\\sqcif\\cg\\cpURegion.bin";
	//String dir = "/h/g3/dhjeon/trunk/test/parasites/pyrprof/ft";
	String dir = System.getProperty("user.dir");
	String sFile = dir + "/sregions.txt";
	String dFile = dir + "/cpURegion.bin";
	JLabel sLabel;
	JLabel dLabel;
	public JButton loadButton;
	public JButton recommendButton;	
	
	ActionListener parent;
	
	
	public ButtonPanel(ActionListener parent) {
		super(new GridLayout(0, 2));
		this.parent = parent;
		loadSButton = new JButton("Load SFile");
		loadDButton = new JButton("Load DFile");
		recommendButton = new JButton("Recommend");
		loadButton = new JButton("Load");
		fc = new JFileChooser();
		loadSButton.addActionListener(this);
		loadDButton.addActionListener(this);
		loadButton.addActionListener(parent);
		recommendButton.addActionListener(parent);
		this.sLabel = new JLabel(sFile);
		this.dLabel = new JLabel(dFile);
		this.add(loadSButton);
		this.add(sLabel);
		this.add(loadDButton);
		this.add(dLabel);
		this.add(loadButton);
		this.add(recommendButton);
	}
	
	public void actionPerformed(ActionEvent e) {
		System.out.println(e);
		 if (e.getSource() == loadSButton) {
	            int returnVal = fc.showOpenDialog(this);
	            if (returnVal == JFileChooser.APPROVE_OPTION) {
	                File file = fc.getSelectedFile();
	                this.sFile = file.getAbsolutePath();
	                this.sLabel.setText(this.sFile);
	            }
		 }
		 
		 if (e.getSource() == loadDButton) {
			int returnVal = fc.showOpenDialog(this);
            if (returnVal == JFileChooser.APPROVE_OPTION) {
                File file = fc.getSelectedFile();
                this.dFile = file.getAbsolutePath();
                this.dLabel.setText(this.dFile);
            }	            
		 }		
	}
	
	public String getSFileName() {
		return this.sFile;
	}
	
	public String getDFileName() {
		return this.dFile;
	}
		
}

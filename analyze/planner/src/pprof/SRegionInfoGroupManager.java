package pprof;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.*;
public class SRegionInfoGroupManager {
	List<SRegionInfoGroup> list;
	Map<SRegionInfo, SRegionInfoGroup> map;
	
	public SRegionInfoGroupManager(SRegionInfoAnalyzer analyzer) {
		this.map = new HashMap<SRegionInfo, SRegionInfoGroup>();
		this.list = build(analyzer);		
		
	}
	
	static final Comparator<SRegionInfoGroup> GPROF_ORDER =
        new Comparator<SRegionInfoGroup>() {
			public int compare(SRegionInfoGroup e1, SRegionInfoGroup e2) {
				return (int)(e2.getEntry(0).getTotalWork() - e1.getEntry(0).getTotalWork()); 
			}
	};
	
	public SRegionInfoGroup getGroup(SRegionInfo info) {
		if (map.containsKey(info) == false) {
			System.out.println(info);			
			assert(false);
		}
		return map.get(info);
	}
	
	public List<SRegionInfoGroup> getGprofList() {
		List<SRegionInfoGroup> groupList = new ArrayList<SRegionInfoGroup>(list);
		Collections.sort(groupList, GPROF_ORDER);
		return groupList;
	}
		
	
	public List<SRegionInfoGroup> getPyrprofList() {
		List<SRegionInfoGroup> groupList = new ArrayList<SRegionInfoGroup>(list);
		Collections.sort(groupList);
		return groupList;
	}
	
	List<SRegionInfoGroup> build(SRegionInfoAnalyzer analyzer) {
		List<SRegionInfoGroup> ret = new ArrayList<SRegionInfoGroup>();
		
		Set<SRegion> retired = new HashSet<SRegion>();
		
		List<SRegion> workList = new ArrayList<SRegion>();		
		workList.add(analyzer.getRootInfo().getSRegion());
		
		while (workList.size() > 0) {
			SRegion toRemove = workList.remove(0);
			if (retired.contains(toRemove))
				continue;
			
			SRegionInfo info = analyzer.getSRegionInfo(toRemove);
			retired.add(toRemove);
			
			long work = info.getTotalWork();
			
			List<SRegionInfo> tempList = new ArrayList<SRegionInfo>();
			tempList.add(info);
			
			List<SRegion> list2 = new ArrayList<SRegion>();
			if (info.getChildren().size() == 1) {
				//tempList.add(info.getChildren().iterator().next());
				SRegion entry = info.getChildren().iterator().next();
				SRegionInfo entryInfo = analyzer.getSRegionInfo(entry);
				if (entryInfo.getParents().size() == 1)
					list2.add(entry);
			}
			
			while(!list2.isEmpty()) {
				SRegion child = list2.remove(0);
				SRegionInfo childInfo = analyzer.getSRegionInfo(child);
				tempList.add(childInfo);
				if (childInfo.getChildren().size() == 1) {
					SRegion entry = childInfo.getChildren().iterator().next();
					SRegionInfo entryInfo = analyzer.getSRegionInfo(entry);
					if (entryInfo.getParents().size() == 1) {
						list2.add(entry);
						continue;
					}
				}
				
				for (SRegion each : childInfo.getChildren()) {
					//SRegionInfo entryInfo = analyzer.getSRegionInfo(each);
					if (!retired.contains(each))
						workList.add(each);
				}
			}
			
			SRegionInfoGroup group = new SRegionInfoGroup(tempList);
			ret.add(group);
			for (SRegionInfo each : tempList) {
				retired.add(each.getSRegion());
				map.put(each, group);
			}
			
			for (SRegion child : info.getChildren()) {
				if (!retired.contains(child))
					workList.add(child);
			}
			
		}
		
		// set parent / child relationship
		for (SRegionInfoGroup each : ret) {
			//SRegionInfo first = each.getEntry(0);
			SRegionInfo last = each.getEntry(each.size() - 1);
			
			Set<SRegion> nexts = last.getChildren();
			for (SRegion next : nexts) {
				SRegionInfo nextInfo = analyzer.getSRegionInfo(next);
				SRegionInfoGroup nextGroup = map.get(nextInfo);
				each.children.add(nextGroup);
				nextGroup.parents.add(each);
			}			
		}
		
		return ret;
	}
}

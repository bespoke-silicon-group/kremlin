package target;
import java.util.*;

import planner.*;
import pprof.*;

public interface ITarget {	
	List<SpeedupRecord> getSpeedupList(SRegionInfo info, int maxCore);	
	
}

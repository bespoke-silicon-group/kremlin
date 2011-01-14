package target;
import java.util.*;
import pprof.*;
import pyrplan.*;

public interface ITarget {	
	List<SpeedupRecord> getSpeedupList(SRegionInfo info, int maxCore);	
	
}

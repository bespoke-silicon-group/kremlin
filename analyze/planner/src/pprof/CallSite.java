package pprof;

public class CallSite extends SRegion {
	CallSite(long id, String name, String module, String func, int start, int end, RegionType type) {
		super(id, name, module, func, start, end, type);
	}
}

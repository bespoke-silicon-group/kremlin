#ifndef PROGRAM_REGION_H
#define PROGRAM_REGION_H

#include "ktypes.h"

class ProgramRegion {
  private:
	static const uint32_t ERROR_CHECK_CODE = 0xDEADBEEF;

  public:
	uint32_t code;
	Version version;
	SID	regionId;
	RegionType regionType;
	Time start;
	Time cp;
	Time childrenWork;
	Time childrenCP;
	Time childMaxCP;
	uint64_t childCount;
#ifdef EXTRA_STATS
	uint64_t loadCnt;
	uint64_t storeCnt;
	uint64_t readCnt;
	uint64_t writeCnt;
	uint64_t readLineCnt;
	uint64_t writeLineCnt;
#endif

	ProgramRegion() : code(ProgramRegion::ERROR_CHECK_CODE), version(0), regionId(0), 
				regionType(RegionFunc), start(0), cp(0), 
				childrenWork(0), childrenCP(0), childMaxCP(0), 
				childCount(0) {}

	void init(SID sid, RegionType regionType, Level level, Time start_time) {
		regionId = sid;
		start = start_time;
		cp = 0ULL;
		childrenWork = 0LL;
		childrenCP = 0LL;
		childMaxCP = 0LL;
		childCount = 0LL;
		this->regionType = regionType;
#ifdef EXTRA_STATS
		loadCnt = 0LL;
		storeCnt = 0LL;
		readCnt = 0LL;
		writeCnt = 0LL;
		readLineCnt = 0LL;
		writeLineCnt = 0LL;
#endif
	}

	void sanityCheck() {
		assert(code == ProgramRegion::ERROR_CHECK_CODE);
	}

	void updateCriticalPathLength(Timestamp value);
};

#endif

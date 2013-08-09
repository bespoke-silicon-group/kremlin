#ifndef PROGRAM_REGION_H
#define PROGRAM_REGION_H

#include "ktypes.h"
#include "PoolAllocator.hpp"
#include <vector>

class ProgramRegion {
  private:
	static std::vector<ProgramRegion*, MPoolLib::PoolAllocator<ProgramRegion*> > program_regions;
	static unsigned int arraySize;
	static Version* vArray;
	static Time* tArray;
	static Version nextVersion;
	static const UInt32 ERROR_CHECK_CODE = 0xDEADBEEF;

  public:
	UInt32 code;
	Version version;
	SID	regionId;
	RegionType regionType;
	Time start;
	Time cp;
	Time childrenWork;
	Time childrenCP;
	Time childMaxCP;
	UInt64 childCount;
#ifdef EXTRA_STATS
	UInt64 loadCnt;
	UInt64 storeCnt;
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 writeLineCnt;
#endif

	ProgramRegion() : code(ProgramRegion::ERROR_CHECK_CODE), version(0), regionId(0), 
				regionType(RegionFunc), start(0), cp(0), 
				childrenWork(0), childrenCP(0), childMaxCP(0), 
				childCount(0) {}

	void init(SID sid, RegionType regionType, Level level, Time start_time) {
		ProgramRegion::issueVersionToLevel(level);

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

	static ProgramRegion* getRegionAtLevel(Level l) {
		assert(l < program_regions.size());
		ProgramRegion* ret = program_regions[l];
		ret->sanityCheck();
		return ret;
	}

	static void increaseNumRegions(unsigned num_new) {
		for (unsigned i = 0; i < num_new; ++i) {
			program_regions.push_back(new ProgramRegion());
		}
	}

	static unsigned getNumRegions() { return program_regions.size(); }
	static void doubleNumRegions() {
		increaseNumRegions(program_regions.size());
	}

	static void initProgramRegions(unsigned num_regions) {
		assert(program_regions.empty());
		increaseNumRegions(num_regions);

		initVersionArray();
		initTimeArray();
	}

	static void deinitProgramRegions() { program_regions.clear(); }

	static void initVersionArray() {
		vArray = new Version[arraySize];
		for (unsigned i = 0; i < arraySize; ++i) vArray[i] = 0;
	}

	static void initTimeArray() {
		tArray = new Time[arraySize];
		for (unsigned i = 0; i < arraySize; ++i) tArray[i] = 0;
	}

	static Time* getTimeArray() { return tArray; }
	static Version* getVersionAtLevel(Level level) { return &vArray[level]; }

	static void issueVersionToLevel(Level level) {
		vArray[level] = nextVersion++;	
	}

};

#endif

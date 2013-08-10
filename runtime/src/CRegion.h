#ifndef _CREGION_H_
#define _CREGION_H_

#include "ktypes.h"

class RegionField {
public:
	UInt64 work;
	UInt64 cp;
	UInt64 callSite;
	UInt64 spWork;
	UInt64 isDoall;
	UInt64 childCnt;
#ifdef EXTRA_STATS
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 writeLineCnt;
	UInt64 loadCnt;
	UInt64 storeCnt;
#endif
};

void CRegionInit();
void CRegionDeinit(const char* file);
void CRegionEnter(SID sid, CID callSite, RegionType type);
void CRegionExit(RegionField* info);

#endif

#ifndef _CREGION_H_
#define _CREGION_H_

#include <vector>

#include "ktypes.h"

class CNode;

class CStat {
public:
	UInt64 totalWork;
	double minSP;
	double maxSP;

	UInt64 spWork;
	UInt64 tpWork;

	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 loadCnt;
	UInt64 storeCnt;
	
	UInt64 totalIterCount;
	UInt64 minIterCount;
	UInt64 maxIterCount;

	UInt64 numInstance;
};

class CTree {
public:
	UInt64 id;
	int maxDepth;
	int currentDepth;
	CNode* root;
	CNode* parent;
}; 

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

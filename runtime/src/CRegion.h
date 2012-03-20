#ifndef _CREGION_H_
#define _CREGION_H_

#include "ktypes.h"

typedef struct _cregion_t {
	UInt64 id;
	UInt64 sid;
	UInt64 callSite;
	UInt64 totalWork;
	UInt64 totalCP;
	double minSP;
	double maxSP;

	UInt64 totalChildCount;
	UInt64 minChildCount;
	UInt64 maxChildCount;

	UInt64 tpWork;
	UInt64 spWork;

	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 loadCnt;
	UInt64 storeCnt;
	
	UInt64 numInstance;
	UInt64 childNum;
	UInt64 isDoall;

} CRegion;

typedef struct _cnode_t CNode;

struct _cnode_t {
	UInt32 code;
	CNode* parent;
	CNode* firstChild;
	CNode* next;	
	UInt64 childrenSize;
	CRegion* region;
};

typedef struct _RegionField_t {
	UInt64 work;
	UInt64 cp;
	UInt64 callSite;
	UInt64 spWork;
	UInt64 tpWork;
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
} RegionField;


void CRegionInit();
void CRegionDeinit(char* file);
void CRegionEnter(SID sid, CID callSite);
void CRegionLeave(RegionField* info);

#endif

#ifndef _UDR_H_
#define _UDR_H_

#include "defs.h"

typedef struct _URegionField_t {
	UInt64 work;
	UInt64 cp;
#ifdef EXTRA_STATS
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 writeLineCnt;
#endif
	UInt64 callSite;
} URegionField;

typedef struct _URegion_t {
	UInt64 uid;
	UInt64 sid;
	UInt64 cnt;
	URegionField field;
	UInt64 childrenSize;
	struct _ChildInfo_* cHeader;	
	struct _URegion_t* next;
} URegion;


typedef struct _DRegion_t {
	UInt64 sid;
	UInt64 did;
	UInt64 pSid;
	UInt64 pDid;
	URegionField field;
	URegion* uregion;
	struct _DRegion_t* next; // for stack
	
} DRegion;

typedef struct _ChildInfo_ {
	//URegion* uregion;
	UInt64 uid;				  // for sorting
	UInt64 cnt;
	struct _ChildInfo_* next; // for sorted list
} ChildInfo;



void processUdr(UInt64 sid, UInt64 did, UInt64 pSid, UInt64 pDid, 
	URegionField field);

void initializeUdr();
void finalizeUdr();

#endif

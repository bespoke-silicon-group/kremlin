#ifndef _CSTAT_H_
#define _CSTAT_H_

#include "ktypes.h"

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

	static CStat* create(int index);
	static void destroy(CStat* region);
};

#endif

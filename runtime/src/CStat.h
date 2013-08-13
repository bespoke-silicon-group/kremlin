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

	UInt64 num_instances;

	CStat() : totalWork(0), minSP(-1), maxSP(0), spWork(0), tpWork(0), readCnt(0),
				writeCnt(0), loadCnt(0), storeCnt(0), totalIterCount(0),
				minIterCount(-1), maxIterCount(0), num_instances(0) {}
	~CStat() {}

	static void* operator new(size_t size);
	static void operator delete(void* ptr);
};

#endif

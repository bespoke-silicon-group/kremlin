#include "CStat.h"
#include "MemMapAllocator.h"

CStat* CStat::create() {
	CStat* ret = (CStat*)MemPoolAllocSmall(sizeof(CStat));
	ret->totalWork = 0;
	ret->tpWork = 0;
	ret->spWork = 0;
	ret->minSP = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxSP = 0;
	ret->readCnt = 0;
	ret->writeCnt = 0;
	ret->loadCnt = 0;
	ret->storeCnt = 0;

	// iteration info	
	ret->totalIterCount = 0;
	ret->minIterCount = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxIterCount = 0;

	ret->numInstance = 0;

	return ret;
}

void CStat::destroy(CStat* stat) {
	MemPoolFreeSmall(stat, sizeof(CStat));
}

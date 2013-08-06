#include "debug.h"
#include "TagVectorCacheLine.h"

static const int TV_CACHE_LINE_DEBUG_LVL = 0;

// XXX: not sure this should be a global function (in MShadowCache.cpp)
extern int getStartInvalidLevel(Version lastVer, Version* vArray, Index size);

bool TagVectorCacheLine::isHit(Addr addr) {
	// XXX: tag printed twice??? (-sat)
	MSG(3, "isHit addr = 0x%llx, tag = 0x%llx, entry tag = 0x%llx\n",
		addr, this->tag, this->tag);

	return (((UInt64)this->tag ^ (UInt64)addr) >> 3) == 0;
}

void TagVectorCacheLine::validateTag(Time* destAddr, Version* vArray, Index size) {
	int firstInvalid = getStartInvalidLevel(this->version[0], vArray, size);

	MSG(TV_CACHE_LINE_DEBUG_LVL, "\t\tTVCacheValidateTag: invalid from level %d\n", firstInvalid);
	if (size > firstInvalid)
		bzero(&destAddr[firstInvalid], sizeof(Time) * (size - firstInvalid));
}


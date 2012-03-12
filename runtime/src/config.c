#include <stdio.h>
#include "config.h"

typedef struct _config_ {
	Level minLevel;
	Level maxLevel;

	UInt8 useCompression;
	UInt8 shadowType;
	UInt8 useVerify;

	UInt32 regionDepth;
	UInt32 cbufferSize;
	UInt32 cacheSize;

	UInt32 gcPeriod;	// garbage collection period 
								    
} KConfig;

static KConfig config;


void KConfigInit() {
	config.useCompression = 0;
	config.useVerify = 0;
	config.minLevel = 0;
	config.maxLevel = 32;
	config.cbufferSize = 4096;
	config.cacheSize = 4;
	config.regionDepth = 20;
	config.shadowType = 2;
	config.gcPeriod = 1024;
}


void   KConfigSetMinLevel(Level level) {
	config.minLevel = level;
}

void   KConfigSetMaxLevel(Level level) {
	config.maxLevel = level;
}

Level  KConfigGetIndexSize() {
	return config.maxLevel - config.minLevel + 1;
}


Level KConfigGetMinLevel() {
	return config.minLevel;
}

Level KConfigGetMaxLevel() {
	return config.maxLevel;
}


void KConfigSetCompression(int compress) {
	config.useCompression = compress;
}

Bool KConfigGetCompression() {
	return config.useCompression;	
}

void KConfigSetCBufferSize(int size) {
	config.cbufferSize = size;
	fprintf(stderr, "setting CBufferSize to %d\n", size);
}

UInt32 KConfigGetCBufferSize() {
	return config.cbufferSize;
}


void KConfigSetRegionDepth(int depth) {
	fprintf(stderr, "[kremlin] Setting region depth to %d\n", depth);
	config.regionDepth = depth;
	if (depth > KConfigGetMaxLevel())
		KConfigSetMaxLevel(depth);
}

UInt32 KConfigGetRegionDepth() {
	return config.regionDepth;
}

void KConfigSetCacheSize(int nMB) {
	config.cacheSize = nMB;
	fprintf(stderr, "[kremlin] Setting cache size to %d MB\n",nMB);
}

UInt32 KConfigGetCacheSize() {
	return config.cacheSize;
}

void KConfigSetShadowType(int n) {
	config.shadowType = n;
}

UInt32 KConfigGetShadowType() {
	return config.shadowType;
}

UInt32 KConfigGetGCPeriod() {
	return config.gcPeriod;
}

void KConfigSetGCPeriod(UInt32 period) {
	config.gcPeriod = period;
}

#if 0
void setTableType(int type) {
	_tableType = type;
}
#endif

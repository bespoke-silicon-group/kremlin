#include <stdio.h>
#include "config.h"

typedef struct _config_ {
	Bool  limitLevel;
	Level minLevel;
	Level maxLevel;

	UInt8 useCompression;
	UInt8 shadowType;
	UInt8 useVerify;
	UInt8 enableRecursion;
	UInt8 enableCRegion;

	UInt32 cbufferSize;
	UInt32 cacheSize;
	char  outFileName[64];

	UInt32 gcPeriod;	// garbage collection period 
								    
} KConfig;

static KConfig config;


void KConfigInit() {
	config.limitLevel = 0;
	config.useCompression = 0;
	config.useVerify = 0;
	config.minLevel = 0;
	config.maxLevel = 32;
	config.cbufferSize = 4096;
	config.cacheSize = 4;
	config.shadowType = 1;
	config.gcPeriod = 1024;
	config.enableRecursion = 1;
	config.enableCRegion = 1;
	strcpy(config.outFileName, "kremlin.bin");
}

void KConfigSetOutFileName(char* name) {
	strcpy(config.outFileName, name);
}

const char* KConfigGetOutFileName() {
	return config.outFileName;	
}

void KConfigDisableRSummary() {
	config.enableRecursion = 0;
}

void KConfigDisableCRegion() {
	config.enableCRegion = 0;
}

Bool KConfigGetRSummarySupport() {
	return config.enableRecursion;
}

Bool KConfigGetCRegionSupport() {
	return config.enableCRegion;
}


Bool KConfigLimitLevel() {
	return config.limitLevel;
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


void KConfigSetSkaduCacheSize(int nMB) {
	config.cacheSize = nMB;
	fprintf(stderr, "[kremlin] Setting cache size to %d MB\n",nMB);
}

UInt32 KConfigGetSkaduCacheSize() {
	return config.cacheSize;
}

UInt32 KConfigUseSkaduCache() {
	return (config.cacheSize > 0);
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

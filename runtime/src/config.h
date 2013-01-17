#ifndef _CONFIG_H
#define _CONFIG_H

#include "ktypes.h"

void   KConfigSetCompression(int);
Bool   KConfigGetCompression();

void   KConfigSetCBufferSize(int);
UInt32 KConfigGetCBufferSize();

void   KConfigSetRegionDepth(int);
UInt32 KConfigGetRegionDepth();

void   KConfigSetShadowType(int);
UInt32 KConfigGetShadowType();

void   KConfigSetSkaduCacheSize(int);
UInt32 KConfigGetSkaduCacheSize();
Bool   KConfigUseSkaduCache();


void   KConfigSetMinLevel(Level);
void   KConfigSetMaxLevel(Level);

Level  KConfigGetMinLevel();
Level  KConfigGetMaxLevel();
Level  KConfigGetIndexSize();

void   KConfigSetGCPeriod(UInt32);
UInt32 KConfigGetGCPeriod();

void KConfigDisableRSummary();
Bool KConfigGetRSummarySupport();

void KConfigSetOutFileName(char* name);
const char* KConfigGetOutFileName();

#endif

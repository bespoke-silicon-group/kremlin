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

void   KConfigSetCacheSize(int);
UInt32 KConfigGetCacheSize();

void   KConfigSetMinLevel(Level);
void   KConfigSetMaxLevel(Level);

Level  KConfigGetMinLevel();
Level  KConfigGetMaxLevel();
Level  KConfigGetIndexSize();


#endif

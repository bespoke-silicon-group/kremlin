#include "kremlin.h"

void  TVCacheSet(Addr addr, Index size, Version* vArray, Time* tArray, int type);
Time* TVCacheGet(Addr addr, Index size, Version* vArray, int type);
int   TVCacheInit(int size);
void  TVCacheDeinit();

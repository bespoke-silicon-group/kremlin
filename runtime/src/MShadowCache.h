#include "kremlin.h"
#include "MShadowSkadu.h"

void  TVCacheSet(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type);
Time* TVCacheGet(Addr addr, Index size, Version* vArray, TimeTable::TableType type);
int   TVCacheInit(int size);
void  TVCacheDeinit();

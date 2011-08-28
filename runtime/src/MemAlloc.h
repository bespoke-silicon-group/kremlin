#ifndef _MEMALLOC
#define _MEMALLOC

void MemAllocInit(int size);
void* MemAlloc();
void MemFree(void* packet);
void MemAllocDeinit();


#endif

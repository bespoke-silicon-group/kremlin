#ifndef MEM_MAP_ALLOCATOR_H
#define MEM_MAP_ALLOCATOR_H

#include <stdlib.h>
#include "ktypes.h"

void MemPoolInit(int, int);
Addr MemPoolAlloc(void);
void MemPoolFree(Addr addr);

Addr MemPoolAllocSmall(int);
Addr MemPoolCallocSmall(int, int);
void MemPoolFreeSmall(Addr addr, int size);
#endif /* MEM_MAP_ALLOCATOR_H */

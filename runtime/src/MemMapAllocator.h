#ifndef MEM_MAP_ALLOCATOR_H
#define MEM_MAP_ALLOCATOR_H

#include <stdlib.h>
#include "defs.h"

Addr MemPoolAlloc(void);
void MemPoolFree(Addr addr);

#endif /* MEM_MAP_ALLOCATOR_H */

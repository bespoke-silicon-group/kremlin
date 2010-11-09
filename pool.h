#ifndef POOL_H
#define POOL_H

#include <stdlib.h>

typedef struct Pool Pool;

int PoolCreate(Pool** p, size_t pageCount, size_t pageSize);
int PoolDelete(Pool** p);

void* PoolMalloc(Pool* p);
void PoolFree(Pool* p, void* ptr);

#endif /* POOL_H */

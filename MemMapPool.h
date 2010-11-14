#ifndef MEM_MAP_POOL_H
#define MEM_MAP_POOL_H

typedef struct MemMapPool MemMapPool;

/**
 * Creates a pool of memory backed by mmap.
 *
 * @param p 			Where to allocate the pool. *p will point to the newly 
 * 						created pool.
 * @param size			The size to allocate.
 * @return 				TRUE on success.
 */
int MemMapPoolCreate(MemMapPool** p, unsigned long long size);

/**
 * Allocates a chunk of memory from the MemMapPool.
 * @param p				The pool.
 * @param size			The amount to allocate.
 * @return 				The address of the allocated memory.
 */
void* MemMapPoolMalloc(MemMapPool* p, unsigned long long size);

#endif /* MEM_MAP_POOL_H */

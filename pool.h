#ifndef POOL_H
#define POOL_H

#include <stdlib.h>
#include "vector.h"

typedef struct Pool Pool;

/**
 * Creates a new fixed-size page pool.
 * @param p             Where to allocate the pool. *p will point to the newly 
 *                      created pool.
 * @param pageSize      The fixed size of the pages in this pool.
 * @param allocator     An memory allocator object.
 * @param mallocFunc    A function that returns a pointer to newly allocated
 *                      memory. The function will be passed the allocator and
 *                      a requested size.
 * @return TRUE on success.
 */
int PoolCreate(Pool** p, size_t pageSize, void* allocator, void* (*mallocFunc)(void*, size_t));

/**
 * Deallocates the pool. All memory allocated in the pool will be invalid
 * after this call.
 *
 * @param p             Address of the pool pointer. It will be set to NULL.
 * @return TRUE on success.
 */
int PoolDelete(Pool** p);

/**
 * Allocates a page from the pool.
 * @param p             The pool.
 * @return              A pointer to the allocated page in the pool.
 */
void* PoolMalloc(Pool* p);

/**
 * Frees a page in the pool.
 * @param p             The pool.
 * @param ptr           The pointer returned from PoolMalloc.
 */
void PoolFree(Pool* p, void* ptr);

/**
 * Returns the page size.
 * @param p             The pool.
 * @return              The size of the pages allocated in this pool.
 */
size_t PoolGetPageSize(Pool* p);

#endif /* POOL_H */

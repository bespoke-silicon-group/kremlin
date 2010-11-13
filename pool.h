#ifndef POOL_H
#define POOL_H

#include <stdlib.h>
#include "vector.h"

struct Pool
{   
	unsigned long signature;
    /// The size of the memory returned from this pool.
    size_t pageSize;

    /// The number of pages in the pool.
    size_t pageCount;

    /// The list of free pages.
    vector* freeList;

    /// Sub-pools of memory this pool can alloc/free memory from.
    vector* subPools;
};

typedef struct Pool Pool;

/**
 * Creates a new fixed-size page pool.
 * @param p 		Where to allocate the pool. *p will point to the newly 
 * 					created pool.
 * @param pageCount A suggesion on how many pages are expected.
 * @param pageSize	The fixed size of the pages in this pool.
 * @return TRUE on success.
 */
int PoolCreate(Pool** p, size_t pageCount, size_t pageSize);

/**
 * Deallocates the pool. All memory allocated in the pool will be invalid
 * after this call.
 *
 * @param p			Address of the pool pointer. It will be set to NULL.
 * @return TRUE on success.
 */
int PoolDelete(Pool** p);

/**
 * Allocates a page from the pool.
 * @param p 		The pool.
 * @return			A pointer to the allocated page in the pool.
 */
void* PoolMalloc(Pool* p);

/**
 * Frees a page in the pool.
 * @param p			The pool.
 * @param ptr		The pointer returned from PoolMalloc.
void PoolFree(Pool* p, void* ptr);

/**
 * Reserves at least pageCount pages for the pool.
 * @param p			The pool.
 * @param pageCount	The amount of pages to allocate now.
 */
void PoolReserve(Pool* p, size_t pageCount);

#endif /* POOL_H */

#ifndef MEM_MAP_ALLOCATOR_H
#define MEM_MAP_ALLOCATOR_H

#include <stdlib.h>
#include "defs.h"

typedef struct MemMapAllocator MemMapAllocator;

/**
 * Creates a memory allocator backed by memmap.
 *
 * @param p 		*p will point to the newly allocated allocator or NULL if 
 * 					creation fails.
 * @param size 		The chunk size to grow the pool by.
 * @return TRUE on success.
 */
int MemMapAllocatorCreate(MemMapAllocator** p, size_t size);

/**
 * Deletes the allocator.
 *
 * @return TRUE on success.
 */
int MemMapAllocatorDelete(MemMapAllocator** p);

/**
 * Allocate some memory from the allocator.
 */
void* MemMapAllocatorMalloc(MemMapAllocator* p, size_t size);

/**
 * Free memory allocated from the allocator.
 */
void MemMapAllocatorFree(MemMapAllocator* p, void* ptr);

void* MemPoolAlloc(void);
void MemPoolFree(void* addr);

#endif /* MEM_MAP_ALLOCATOR_H */

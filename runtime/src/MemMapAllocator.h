#ifndef MEM_MAP_ALLOCATOR_H
#define MEM_MAP_ALLOCATOR_H

#include <stdlib.h>

typedef struct MemMapAllocator MemMapAllocator;

int MemMapAllocatorCreate(MemMapAllocator** p, size_t size);
int MemMapAllocatorDelete(MemMapAllocator** p);
void* MemMapAllocatorMalloc(MemMapAllocator* p, size_t size);
void MemMapAllocatorFree(MemMapAllocator* p, void* ptr);

#endif /* MEM_MAP_ALLOCATOR_H */

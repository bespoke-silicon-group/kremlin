/**
 * @file MemMapMemMapAllocator.c
 * @brief Defines a pool of memory backed by mmap.
 */

#define _GNU_SOURCE
#include "MemMapAllocator.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>

#define TRUE 1
#define FALSE 0

#ifdef MAP_ANON
#   define MEM_MAP_POOL_ANON MAP_ANON
#else
#   ifdef MAP_ANONYMOUS
#       define MEM_MAP_POOL_ANON MAP_ANONYMOUS
#   else
#       error MAP_ANON or MAP_ANONYMOUS are required for MemMapAllocator
#   endif /* MAP_ANONYMOUS */
#endif /* MAP_ANON */

/**
 * MemMapAllocator using mmap.
 *
 * Currently, no freeing.
 */
struct MemMapAllocator
{
    /// The memory the pool can allocate from.
    unsigned char* data;

    /// Points to the next memory location to allocate.
    unsigned char* freeListHead;

    /// The size of the pool in bytes.
    size_t size;
};

/* -------------------------------------------------------------------------- 
 * Functions (alpha order).
 * ------------------------------------------------------------------------*/
int MemMapAllocatorCreate(MemMapAllocator** p, size_t size)
{
    int protection = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MEM_MAP_POOL_ANON;
    int fileId = -1;
    int offset = 0;

    if(!((*p = (MemMapAllocator*)malloc(sizeof(MemMapAllocator)))))
    {
        assert(0 && "MemMapAllocatorCreate - malloc failed.");
        return FALSE;
    }

    // Allocate mmapped data.
    if(((*p)->data = (unsigned char*)mmap(NULL, size, protection, flags, fileId, offset)) == MAP_FAILED)
    {
        char* errorString;
        asprintf(&errorString, "MemMapAllocatorCreate - unable to mmap %llu bytes", (unsigned long long)size);
        perror(errorString);
        free(errorString);
        errorString = NULL;
        assert(0);
        free(*p);
        *p = NULL;
        return FALSE;
    }

    // Initialize fields.
    (*p)->size = size;
    (*p)->freeListHead = (*p)->data;

    return TRUE;
}

int MemMapAllocatorDelete(MemMapAllocator** p)
{
	munmap((*p)->data, (*p)->size);
	free(*p);
	p = NULL;
}

void* MemMapAllocatorMalloc(MemMapAllocator* p, size_t size)
{
    void* ret = p->freeListHead;
    unsigned char* lastPtr = p->freeListHead;
    p->freeListHead += size;
    assert(lastPtr < p->freeListHead && "Overflow detected!");
    assert(p->freeListHead < p->data + p->size && "MemMapAllocator ran out of memory.");
    return ret;
}

void MemMapAllocatorFree(MemMapAllocator* p, void* ptr)
{
}

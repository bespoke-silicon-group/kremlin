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
#include "vector.h"

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
    vector* data;

    /// Points to the next memory location to allocate.
    unsigned char* freeListHead;

    /// The size of the pool in bytes.
    size_t size;
};

int MemMapAllocatorAllocAdditional(MemMapAllocator* p);

/* -------------------------------------------------------------------------- 
 * Functions (alpha order).
 * ------------------------------------------------------------------------*/
int MemMapAllocatorAllocAdditional(MemMapAllocator* p)
{
    int protection = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MEM_MAP_POOL_ANON;
    int fileId = -1;
    int offset = 0;

    // Allocate mmapped data.
	unsigned char* data;
    if((data = (unsigned char*)mmap(NULL, p->size, protection, flags, fileId, offset)) == MAP_FAILED)
    {
        char* errorString;
        asprintf(&errorString, "MemMapAllocatorCreate - unable to mmap %llu bytes", (unsigned long long)p->size);
        perror(errorString);
        free(errorString);
        errorString = NULL;
        assert(0);
        return FALSE;
    }

	if(!vector_push(p->data, data))
	{
		assert(0 && "Failed to push initial mmap data");
		return FALSE;
	}

    p->freeListHead = vector_top(p->data);

	return TRUE;
}

int MemMapAllocatorCreate(MemMapAllocator** p, size_t size)
{
    if(!((*p = (MemMapAllocator*)malloc(sizeof(MemMapAllocator)))))
    {
        assert(0 && "MemMapAllocatorCreate - malloc failed.");
        return FALSE;
    }

	if(!vector_create(&(*p)->data, NULL, NULL))
	{
		assert(0 && "MemMapAllocatorCreate - Failed to allocate vector for memmapped data");
		MemMapAllocatorDelete(p);
		return FALSE;
	}

    // Initialize fields.
    (*p)->size = size;

	if(!MemMapAllocatorAllocAdditional(*p))
		return FALSE;

    return TRUE;
}

int MemMapAllocatorDelete(MemMapAllocator** p)
{
	void** data;
	void** data_end;
	for(data = vector_begin((*p)->data), data_end = vector_end((*p)->data); data < data_end; data++)
		munmap(*data, (*p)->size);

	vector_delete(&(*p)->data);

	free(*p);
	p = NULL;

	return TRUE;
}

void* MemMapAllocatorMalloc(MemMapAllocator* p, size_t size)
{
    void* ret = p->freeListHead;
    p->freeListHead += size;
    if(p->freeListHead >= (unsigned char*)vector_top(p->data) + p->size)
	{
		if(!MemMapAllocatorAllocAdditional(p))
			return FALSE;
		return MemMapAllocatorMalloc(p, size);
	}
    return ret;
}

void MemMapAllocatorFree(MemMapAllocator* p, void* ptr)
{
	// TODO: implement
}

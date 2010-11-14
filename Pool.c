#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "Pool.h"
#include "vector.h"

#define TRUE 1
#define FALSE 0

#define MAX_ALLOC_SIZE  (1024*1024*512)
#define DEFAULT_PAGE_COUNT  16

static int PoolFreeListPush(Pool* p, void* ptr);
static void* PoolFreeListPop(Pool* p);

struct Pool
{
    /// The size of the memory returned from this pool.
    size_t pageSize;

    /// The number of pages in the pool.
    size_t pageCount;

    /// Object that allocates memory. Passed to mallocFunc.
    void* allocator;

    /// The function to call when the pool requests more memory.
    void* (*mallocFunc)(void*, size_t);

    /// The list of free pages.
    vector* freeList;
};

/* -------------------------------------------------------------------------- 
 * Functions (alpha order).
 * ------------------------------------------------------------------------*/
int PoolCreate(Pool** p, size_t pageSize, void* allocator, void* (*mallocFunc)(void*, size_t))
{
    assert(sizeof(size_t) >= sizeof(unsigned long long) && "Requires 64-bit size_t");

    if(!((*p) = (Pool*)malloc(sizeof(Pool))))
    {
        assert(0 && "Failed to alloc pool.");
        return FALSE;
    }
    (*p)->allocator = allocator;
    (*p)->mallocFunc = mallocFunc;
    (*p)->pageSize = pageSize;
    (*p)->pageCount = 0; // Field is not used.

    if(!vector_create(&(*p)->freeList, NULL, NULL))
    {
        assert(0 && "Failed to alloc free list.");
        return FALSE;
    }
    
    return TRUE;
}

// XXX: IMPLEMENT PROPERLY
int PoolDelete(Pool** p)
{
    if(!p || !*p)
        return FALSE;

    free(*p);
    *p = NULL;

    return TRUE;
}

void PoolFree(Pool* p, void* ptr)
{
    //assert(ptr >= *p->data && (unsigned char*)ptr < (unsigned char*)p->data + p->pageCount * p->pageSize);

    PoolFreeListPush(p, ptr);
}

int PoolFreeListPush(Pool* p, void* ptr)
{
    return vector_push(p->freeList, ptr);
}

void* PoolFreeListPop(Pool* p)
{
    return vector_pop(p->freeList);
}

size_t PoolGetPageSize(Pool* p)
{
    return p->pageSize;
}

void* PoolMalloc(Pool* p)
{
    void* ret;

    // Try to get something from the free list.
    if(ret = PoolFreeListPop(p))
        return ret;

    // Get a new element and add it to this pool.
    PoolFreeListPush(p, (*p->mallocFunc)(p->allocator, p->pageSize));
    return PoolMalloc(p);
}

void PoolReserve(Pool* p, size_t pageCount)
{
    assert(0 && "Not implemented.");
}


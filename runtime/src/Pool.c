#include "defs.h"
#include <stdlib.h>
#include <stdio.h>
#include "Pool.h"
#include "Vector.h"

#define TRUE 1
#define FALSE 0

#define MAX_ALLOC_SIZE  (1024*1024*512)
#define DEFAULT_PAGE_COUNT  16

VECTOR_DEFINE_PROTOTYPES(FreeList, void*);
VECTOR_DEFINE_FUNCTIONS(FreeList, void*, VECTOR_COPY, VECTOR_NO_DELETE);

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
    FreeList* freeList;
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

    if(!FreeListCreate(&(*p)->freeList))
    {
        assert(0 && "Failed to alloc free list.");
        return FALSE;
    }
    
    return TRUE;
}

int PoolDelete(Pool** p)
{
    if(!p || !*p)
        return FALSE;

    // XXX: The memory pointed to in the free list is never free'd!
    FreeListDelete(&(*p)->freeList);

    free(*p);
    *p = NULL;

    return TRUE;
}

void PoolFree(Pool* p, void* ptr)
{
    PoolFreeListPush(p, ptr);
}

int PoolFreeListPush(Pool* p, void* ptr)
{
    return FreeListPush(p->freeList, ptr);
}

void* PoolFreeListPop(Pool* p)
{
    return FreeListPopVal(p->freeList);
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


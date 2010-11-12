#include "pool.h"
#include "vector.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define TRUE 1
#define FALSE 0

#define MAX_ALLOC_SIZE  (1024*1024*1024)
#define DEFAULT_PAGE_COUNT  16

typedef struct SubPool SubPool;

struct Pool
{
    /// The size of the memory returned from this pool.
    size_t pageSize;

    /// The number of pages in the pool.
    size_t pageCount;

    /// The list of free pages.
    vector* freeList;

    /// Sub-pools of memory this pool can alloc/free memory from.
    vector* subPools;
};

struct SubPool
{
    /// The pool that owns this sub pool.
    Pool* parent;

    /// The size of allocated data.
    size_t size;

    /// The space to allocate from.
    unsigned char data[];
};

int SubPoolCreate(SubPool** p, Pool* parent, size_t size);
int SubPoolDelete(SubPool** p);

int PoolFreeListPush(Pool* p, void* ptr);
void* PoolFreeListPop(Pool* p);

/**
 * Creates a new SubPool. This pool contains pages of size pageSize according
 * the pool this is within.
 *
 * @param p *p will point to the newly allocated pool.
 * @param parent The pool this sub pool belongs to.
 *
 * @return TRUE on success.
 */
int SubPoolCreate(SubPool** p, Pool* parent, size_t size)
{
    unsigned char* page;

    if(!(*p = (SubPool*)malloc(sizeof(SubPool) + size)))
        return FALSE;

    (*p)->parent = parent;
    (*p)->size = size;

    return TRUE;
}

/**
 * Deletes a subpool.
 *
 * This will deallocate any properly constructed structure in the sub pool. If
 * a structure is not constructed, it will be safely ignored.
 */
int SubPoolDelete(SubPool** p)
{
    if(!p || !*p)
        return TRUE;

    free(*p);
    *p = NULL;

    return TRUE;
}

int PoolCreate(Pool** p, size_t pageCount, size_t pageSize)
{
    unsigned char* page;

    assert(sizeof(size_t) >= sizeof(unsigned long long) && "Requires 64-bit size_t");

    if(!((*p) = (Pool*)malloc(sizeof(Pool))))
    {
        assert(0 && "Failed to alloc pool.");
        return FALSE;
    }

    (*p)->pageSize = pageSize;
    (*p)->pageCount = DEFAULT_PAGE_COUNT; // the pageCount argument is ignored because this pool grows dynamically.

    if(!vector_create(&(*p)->freeList, NULL, NULL))
    {
        assert(0 && "Failed to alloc subPool list.");
        return FALSE;
    }
    
    if(!vector_create(&(*p)->subPools, NULL, NULL))
    {
        assert(0 && "Failed to alloc subPool list.");
        return FALSE;
    }

    return TRUE;
}

/**
 * Allocates a new large chunk of memory and adds it to the pool of memory we
 * can allocate from.
 */
int PoolAllocNewSubPool(Pool* p)
{
    unsigned char* page;
    SubPool* newPool;
    size_t pageCount = p->pageCount;
    size_t pageSize = p->pageSize;

    size_t size = pageSize * pageCount;
    if(size > MAX_ALLOC_SIZE)
        size = MAX_ALLOC_SIZE;

    p->pageCount += size / pageSize;

    SubPoolCreate(&newPool, p, size);
    vector_push(p->subPools, newPool);

    // Add the pages from the newly allocated pool to our free list.
    for(page = newPool->data; page < (unsigned char*)newPool->data + (pageCount-1) * pageSize; page += pageSize)
    {
        if(!(PoolFreeListPush(p, page)))
        {
            fprintf(stderr, "Failed to add page to pool's free list!\n");
            assert(0);
        }
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

void* PoolMalloc(Pool* p)
{
    void* ret;

    // Try to get something from the free list.
    if(ret = PoolFreeListPop(p))
        return ret;

    // If the free list had nothing, allocate a new pool and try again.
    PoolAllocNewSubPool(p);
    return PoolMalloc(p);
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

void PoolReserve(Pool* p, size_t pageCount)
{
    while(p->pageCount < pageCount)
        PoolAllocNewSubPool(p);
}


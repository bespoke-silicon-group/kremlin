#include "MemMapPool.h"
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
#       error MAP_ANON or MAP_ANONYMOUS are required for MemMapPool
#   endif /* MAP_ANONYMOUS */
#endif /* MAP_ANON */

/**
 * Pool using mmap.
 *
 * Currently, no freeing.
 */
struct MemMapPool
{
    /// The memory the pool can allocate from.
    unsigned char* data;

    /// Points to the next memory location to allocate.
    unsigned char* freeListHead;

    /// The size of the pool in bytes.
    unsigned long long size;
};

/* -------------------------------------------------------------------------- 
 * Functions (alpha order).
 * ------------------------------------------------------------------------*/
int MemMapPoolCreate(MemMapPool** p, unsigned long long size)
{
    int protection = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MEM_MAP_POOL_ANON;
    int fileId = -1;
    int offset = 0;

    if(!((*p = (MemMapPool*)malloc(sizeof(MemMapPool)))))
    {
        assert(0 && "MemMapPoolCreate - malloc failed.");
        return FALSE;
    }

    // Allocate mmapped data.
    if(((*p)->data = mmap(NULL, size, protection, flags, fileId, offset)) == MAP_FAILED)
    {
        char* errorString;
        asprintf(&errorString, "MemMapPoolCreate - unable to mmap %u bytes\n", size);
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

void* MemMapPoolMalloc(MemMapPool* p, unsigned long long size)
{
    void* ret = p->freeListHead;
    unsigned char* lastPtr = p->freeListHead;
    p->freeListHead += size;
    assert(lastPtr < p->freeListHead && "Overflow detected!");
    assert(p->freeListHead < p->data + p->size && "MemMapPool ran out of memory.");
    return ret;
}


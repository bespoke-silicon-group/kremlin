/**
 * @file MemMapMemMapAllocator.c
 * @brief Defines a pool of memory backed by mmap.
 */

#define _GNU_SOURCE
#include "kremlin.h"
#include "MemMapAllocator.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "mpool.h"

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

typedef struct _MChunk {
	Addr addr;
	struct _MChunk* next;
} MChunk;

static int chunkSize;
static int mmapSizeMB;
static MChunk* freeList;
static mpool_t* poolSmall;


void MemPoolInit(int nMB, int sizeEach) {
	mmapSizeMB = nMB;
	chunkSize = sizeEach;
	freeList = NULL;	
	int error;
	poolSmall = mpool_open(0, 0, 0x100000000000, &error);
	if (error != 1) {
		fprintf(stderr, "mpool_open error = %x\n", error); 
		assert(0);
	}

}

void MemPoolDeinit() {
	mpool_close(poolSmall);
}

Addr MemPoolAllocSmall(int size) {
	//assert(size <= chunkSize);
	//return MemPoolAlloc();
	int error;
	return mpool_alloc(poolSmall, size, &error);
}

Addr MemPoolCallocSmall(int num, int size) {
	int error;
	return mpool_calloc(poolSmall, num, size, &error);
}

void MemPoolFreeSmall(Addr addr, int size) {
	int error = mpool_free(poolSmall, addr, size);
	assert(error = MPOOL_ERROR_NONE);
	//MemPoolFree(addr);
}

MChunk* MChunkAlloc(Addr addr) {
	//MChunk* ret = malloc(sizeof(MChunk));
	MChunk* ret = MemPoolAllocSmall(sizeof(MChunk));
	ret->addr = addr;
	return ret;
}

void MChunkFree(MChunk* target) {
	MemPoolFreeSmall(target, sizeof(MChunk));
}

void addMChunk(MChunk* toAdd) {
	MChunk* head = freeList;
	toAdd->next = head;
	freeList = toAdd;
}

static void FillFreeList() {
	int protection = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MEM_MAP_POOL_ANON;
    int fileId = -1;
    int offset = 0;

    // Allocate mmapped data.
	unsigned char* data;
// Mac OS X doesn't have mmap64... not sure if it's really needed
#ifdef __MACH__
    data = (unsigned char*)mmap(NULL, mmapSizeMB * 1024 * 1024, protection, flags, fileId, offset);
#else
    data = (unsigned char*)mmap64(NULL, mmapSizeMB * 1024 * 1024, protection, flags, fileId, offset);
#endif
	
	if (data == MAP_FAILED) {
		fprintf(stderr, "mmap failed\n");

	} else {
		assert(freeList == NULL);
	}

	unsigned char* current = data;
	int cnt = 0;	
	while ((current + chunkSize) < (data + mmapSizeMB * 1024 * 1024)) {
		//fprintf(stderr, "current = 0x%llx\n", current);
		MChunk* toAdd = MChunkAlloc(current);		
		addMChunk(toAdd);	
		current += chunkSize;
		cnt++;
	}
	fprintf(stderr, "Allocated %d chunks starting at 0x%llx\n", cnt, data);
	fprintf(stderr, "last = 0x%llx\n", current - chunkSize);
}


Addr MemPoolAlloc() {
	if (freeList == NULL) {
		FillFreeList();
	}
	MChunk* head = freeList;
	void* ret = freeList->addr;
	freeList = freeList->next;
	MChunkFree(head);

	//bzero(ret, chunkSize);
	//fprintf(stderr, "Returning addr 0x%llx\n", ret);
	return ret;
}

void MemPoolFree(Addr addr) {
	MChunk* toAdd = MChunkAlloc(addr);
	addMChunk(toAdd);
}



#if 0
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

static int MemMapAllocatorAllocAdditional(MemMapAllocator* p);

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
    if((data = (unsigned char*)mmap64(NULL, p->size, protection, flags, fileId, offset)) == MAP_FAILED)
    {
        char* errorString;
        asprintf(&errorString, "MemMapAllocatorAllocAdditional - unable to mmap %llu bytes", (unsigned long long)p->size);
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
        {
            assert(0 && "Failed to alloc additional");
			return NULL;
        }
		return MemMapAllocatorMalloc(p, size);
	}
    return ret;
}

void MemMapAllocatorFree(MemMapAllocator* p, void* ptr)
{
	// TODO: implement
}
#endif

#include "pool.h"
#include "vector.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define TRUE 1
#define FALSE 0

#define MAX_ALLOC_SIZE	(1024*1024*1024)

struct Pool
{
	size_t pageSize;
	size_t pageCount;
	Pool* next;
	vector* freeList;
	void * highAddr;
	unsigned char data[];
};



int PoolCreate(Pool** p, size_t pageCount, size_t pageSize)
{
	unsigned char* page;

	assert(sizeof(size_t) >= sizeof(unsigned long long));
	size_t size = sizeof(Pool) + pageSize * pageCount;
	if(!((*p) = (Pool*)malloc(sizeof(Pool))))
	{
		assert(0 && "Failed to alloc pool.");
		return FALSE;
	}
	size_t highAddr = ((size_t)*p) + size;
	fprintf(stderr, "PoolCreate: addr = 0x%llx to 0x%llx, size = %lld page = %lld, count = %lld\n", *p, highAddr, size, pageSize, pageCount);
	//bzero(*p, size);

	(*p)->next = NULL;
	(*p)->pageSize = pageSize;
	(*p)->pageCount = pageCount;
	(*p)->highAddr = (void *)highAddr;
	if(!vector_create(&(*p)->freeList, NULL, NULL))
	{
		assert(0 && "Failed to alloc free list.");
		return FALSE;
	}

	if(!vector_reserve((*p)->freeList, pageCount))
	{
		assert(0 && "Failed to reserve space in free list.");
		return FALSE;
	}

	unsigned long long pagesAllocated = 0;
 	size_t memoryToAllocate = pageSize * pageCount;
	while (memoryToAllocate > 0) {
		size_t currentAllocate = (memoryToAllocate < MAX_ALLOC_SIZE) ? memoryToAllocate : MAX_ALLOC_SIZE;
		char *allocated = malloc(currentAllocate);
		char *current;

		fprintf(stderr, "allocated = 0x%llx size = %lld\n", allocated, currentAllocate);
		for (current = allocated; current < allocated + currentAllocate; current +=pageSize) {
			vector_push((*p)->freeList, current);
			pagesAllocated++;
		}
		memoryToAllocate -= currentAllocate;
	}
	fprintf(stderr, "bzero done..\n");
	/*
	for(page = (*p)->data; page < (unsigned char*)(*p)->data + (pageCount-1) * pageSize; page += pageSize)
	{
		vector_push((*p)->freeList, page);
		pagesAllocated++;
	}*/

	fprintf(stderr, "PoolCreate - pagesAllocated: %llu\n", pagesAllocated);

	return TRUE;
}

int PoolDelete(Pool** p)
{
	if(!p || !*p)
		return FALSE;

	if(vector_size((*p)->freeList) != (*p)->pageCount)
	{
		fprintf(stderr, "WARNING: Deleting pool before freeing all memory allocated from the pool.\n");
		fprintf(stderr, "%llu references remain\n.", (unsigned long long)(*p)->pageCount - vector_size((*p)->freeList));
	}
	if(!vector_delete(&(*p)->freeList))
		assert(0 && "vector_delete failed!\n");

	free(*p);
	*p = NULL;

	return TRUE;
}

void* PoolMalloc(Pool* p)
{
	void* ret = vector_pop(p->freeList);
	assert(ret && "Pool ran out of memory");
//	fprintf(stderr, "PoolMalloc addr = 0x%llx\n", ret);
//	assert(ret >= p->data && ret <= p->highAddr);
	
	return ret;
}

void PoolFree(Pool* p, void* ptr)
{

	//assert(ptr >= *p->data && (unsigned char*)ptr < (unsigned char*)p->data + p->pageCount * p->pageSize);

	vector_push(p->freeList, ptr);
}

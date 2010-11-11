#include "pool.h"
#include "vector.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define TRUE 1
#define FALSE 0

struct Pool
{
	size_t pageSize;
	size_t pageCount;
	Pool* next;
	vector* freeList;
	unsigned char data[];
};

int PoolCreate(Pool** p, size_t pageCount, size_t pageSize)
{
	unsigned char* page;

	assert(sizeof(size_t) >= sizeof(unsigned long long));

	if(!((*p) = (Pool*)malloc(sizeof(Pool) + pageSize * pageCount)))
	{
		assert(0 && "Failed to alloc pool.");
		return FALSE;
	}

	(*p)->next = NULL;
	(*p)->pageSize = pageSize;
	(*p)->pageCount = pageCount;
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
	for(page = (*p)->data; page < (unsigned char*)(*p)->data + pageCount * pageSize; page += pageSize)
	{
		vector_push((*p)->freeList, page);
		pagesAllocated++;
	}

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
	return ret;
}

void PoolFree(Pool* p, void* ptr)
{

	assert(ptr >= *p->data && (unsigned char*)ptr < (unsigned char*)p->data + p->pageCount * p->pageSize);

	vector_push(p->freeList, ptr);
}

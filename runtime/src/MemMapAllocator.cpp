/**
 * @file MemMapMemMapAllocator.c
 * @brief Defines a pool of memory backed by mmap.
 */

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
	poolSmall = mpool_open(0, 0, (void*)0x100000000000, &error);
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
	MChunk* ret = (MChunk*)MemPoolAllocSmall(sizeof(MChunk));
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
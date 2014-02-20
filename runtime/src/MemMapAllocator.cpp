/**
 * @file MemMapMemMapAllocator.cpp
 * @brief Defines a pool of memory backed by mmap.
 */

#include <cstdlib>
#include <cstdio>
#include <sys/mman.h>

#include "debug.h"
#include "MemMapAllocator.h"
#include "mpool.h"

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
		fprintf(stderr, "ERROR (mpool_open): %s\n", mpool_strerror(error)); 
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
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;

    // Allocate mmapped data.
	unsigned char* data;
// Mac OS X doesn't have mmap64... not sure if it's really needed
#ifdef __MACH__
    data = (unsigned char*)mmap(NULL, mmapSizeMB * 1024 * 1024, protection,
								flags, -1, 0);
#else
    data = (unsigned char*)mmap64(NULL, mmapSizeMB * 1024 * 1024, protection,
								flags, -1, 0);
#endif
	
	if (data == MAP_FAILED) {
		perror("mmap");
		exit(1);
	} else {
		assert(freeList == NULL);
	}

	unsigned char* current = data;
	unsigned cnt = 0;	
	while ((current + chunkSize) < (data + mmapSizeMB * 1024 * 1024)) {
		//fprintf(stderr, "current = 0x%llx\n", current);
		MChunk* toAdd = MChunkAlloc(current);		
		addMChunk(toAdd);	
		current += chunkSize;
		cnt++;
	}
	MSG(1, "Allocated %u chunks starting at 0x%p\n", cnt, data);
	MSG(1, "last = 0x%p\n", current - chunkSize);
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

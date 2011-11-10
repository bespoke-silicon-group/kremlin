#include "kremlin.h"
#include "debug.h"
#include <stdlib.h>

static int malloc_size;
static char* base;
#define MEMSIZE	((UInt64)1024*1024*1024*4)
//static char base[MEMSIZE];

typedef struct _ListItem {
	Addr addr;
	struct _ListItem* next;
}	ListItem;

typedef struct _List {
	ListItem* head;
	ListItem* tail;
	int num;
} List;

static List* freeList;
static List* dirtyList;

ListItem* allocListItem(Addr addr) {
	ListItem* ret = malloc(sizeof(ListItem));
	ret->addr = addr;
	ret->next = NULL;
	return ret;
}

void freeListItem(ListItem* item) {
	free(item);
}

void addToList(List* list, Addr addr) {
	ListItem* item = allocListItem(addr);
	if (list->head == NULL)
		list->head = item;
	
	if (list->tail == NULL)
		list->tail = item;
	else {
		list->tail->next = item;
		list->tail = item;
	}
	list->num++;
}

Addr popFromList(List* list) {
	ListItem* prevHead = list->head;
	Addr ret = prevHead->addr;
	list->head = prevHead->next;
	list->num--;
	return ret;
}


void MemAllocInit(int size) {
	malloc_size = size;
	UInt64 nItem = MEMSIZE / malloc_size;

	// init lists
	freeList = malloc(sizeof(List));
	dirtyList = malloc(sizeof(List));
	freeList->head = NULL;
	freeList->tail = NULL;
	freeList->num = 0;
	dirtyList->head = NULL;
	dirtyList->tail = NULL;
	dirtyList->num = 0;

#if 0
	base = calloc(nItem, malloc_size);
	int i;
	for (i=0; i<nItem; i++) {
		addToList(freeList, base + i * malloc_size);
	}

	if (base == NULL) {
		fprintf(stderr, "not enough memory..\n");
	} else {
		fprintf(stderr, "memAllocInit succeessful @ 0x%llx size = %d..\n", base, size);
	}
#endif
}

void MemAllocDeinit() {
	free(freeList);
	free(dirtyList);
}

void* MemAlloc() {
#if 0
	assert(freeList->num > 0);
	return popFromList(freeList);
#endif
	return calloc(malloc_size, 1);
}

void MemFree(void* packet) {
	free(packet);
#if 0
	bzero(packet, malloc_size);
	addToList(freeList, packet);
#endif
	//free(packet);
#if 0
	MSG(0, "MemFree: 0x%llx \n", packet);
	assert(packet < (UInt64)base + MEMSIZE);
	assert(packet >= base);
#endif
}


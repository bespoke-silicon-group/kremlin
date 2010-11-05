#include "kmalloc.h"

void* kmalloc(size_t size)
{
	return malloc(size);
}

void kfree(void* ptr)
{
	free(ptr);
}

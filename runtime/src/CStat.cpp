#include "CStat.h"
#include "MemMapAllocator.h"

void* CStat::operator new(size_t size) {
	return (CStat*)MemPoolAllocSmall(sizeof(CStat));
}

void operator delete(void* ptr) {
	MemPoolFreeSmall(ptr, sizeof(CStat));
}

#include <cstddef> // for size_t and
#include "ProfileNodeStats.hpp"
#include "MemMapAllocator.hpp"

void* ProfileNodeStats::operator new(size_t size) {
	return MemPoolAllocSmall(sizeof(ProfileNodeStats));
}

void ProfileNodeStats::operator delete(void *ptr) {
	MemPoolFreeSmall(ptr, sizeof(ProfileNodeStats));
}

#include "kremlin.h"
#include "MShadowDummy.hpp"


Time _dummy_buffer[128];

Time* MShadowDummy::get(Addr addr, Index size, Version* vArray, uint32_t width) {
	return _dummy_buffer;
}

void MShadowDummy::set(Addr addr, Index size, Version* vArray, Time* tArray, uint32_t width) {}

void MShadowDummy::init() {
	fprintf(stderr, "[kremlin] MShadowDummy Init\n");
}

void MShadowDummy::deinit() {}

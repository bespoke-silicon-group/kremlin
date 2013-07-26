#include "kremlin.h"
#include "MShadowDummy.h"


Time _dummy_buffer[128];

Time* MShadowDummy::get(Addr addr, Index size, Version* vArray, UInt32 width) {
	return _dummy_buffer;
}

void MShadowDummy::set(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {}

void MShadowDummy::init() {
	fprintf(stderr, "[kremlin] MShadowDummy Init\n");
}

void MShadowDummy::deinit() {}

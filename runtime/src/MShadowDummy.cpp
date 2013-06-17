#include "kremlin.h"

static void _MShadowSetDummy(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
		
}


Time _dummy_buffer[128];

static Time* _MShadowGetDummy(Addr addr, Index size, Version* vArray, UInt32 width) {
	return _dummy_buffer;
}

void MShadowInitDummy() {
	MShadowGet = _MShadowGetDummy;
	MShadowSet = _MShadowSetDummy;
	fprintf(stderr, "kremlin] MShadowDummy Init\n");
}

void MShadowDeinitDummy() {}

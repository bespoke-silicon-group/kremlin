#include "kremlin.h"

static void _MShadowSetDummy(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
		
}


Time _dummy_buffer[128];

static Time* _MShadowGetDummy(Addr addr, Index size, Version* vArray, UInt32 width) {
	return _dummy_buffer;
}

UInt MShadowInitDummy() {
	MShadowGet = _MShadowGetDummy;
	MShadowSet = _MShadowSetDummy;
	fprintf(stderr, "kremlin] MShadowDummy Init\n");
	return 0;
}

UInt MShadowDeinitDummy() {
	return 0;	
}

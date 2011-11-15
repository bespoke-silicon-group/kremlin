#include "kremlin.h"

static void _MShadowGetDummy(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width) {
		
}


static Time buffer[128];

static Time* _MShadowSetDummy(Addr addr, Index size, Version* vArray, UInt32 width) {
	return buffer;
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

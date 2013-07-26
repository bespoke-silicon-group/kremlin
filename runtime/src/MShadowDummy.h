#ifndef _MSHADOW_DUMMY_H
#define _MSHADOW_DUMMY_H

#include "ktypes.h"
#include "MShadow.h"

class MShadowDummy : public MShadow {
public:
	void init();
	void deinit();

	Time* get(Addr addr, Index size, Version* versions, UInt32 width);
	void set(Addr addr, Index size, Version* versions, Time* times, UInt32 width);
};

#endif

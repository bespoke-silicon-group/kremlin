#ifndef _MSHADOW_BASE_H
#define _MSHADOW_BASE_H

#include "ktypes.h"
#include "MShadow.h"

class MShadowBase : public MShadow {
public:
	void init();
	void deinit();

	Time* get(Addr addr, Index size, Version* versions, UInt32 width);
	void set(Addr addr, Index size, Version* versions, Time* times, UInt32 width);
};

#endif

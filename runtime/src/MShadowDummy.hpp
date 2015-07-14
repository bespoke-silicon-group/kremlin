#ifndef _MSHADOW_DUMMY_H
#define _MSHADOW_DUMMY_H

#include "ktypes.h"
#include "MShadow.hpp"

class MShadowDummy : public MShadow {
public:
	void init();
	void deinit();

	Time* get(Addr addr, Index size, Version* versions, uint32_t width);
	void set(Addr addr, Index size, Version* versions, Time* times, uint32_t width);
};

#endif

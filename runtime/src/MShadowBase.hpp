#ifndef _MSHADOW_BASE_H
#define _MSHADOW_BASE_H

#include "ktypes.h"
#include "MShadow.hpp"

class MShadowBase : public MShadow {
public:
	void init();
	void deinit();

	Time* get(Addr addr, Index size, Version* versions, uint32_t width);
	void set(Addr addr, Index size, Version* versions, Time* times, uint32_t width);
};

#endif

#ifndef _MSHADOW_STV_H
#define _MSHADOW_STV_H

#include "ktypes.h"
#include "MShadow.hpp"

class MShadowSTV : public MShadow {
public:
	void init();
	void deinit();

	Time* get(Addr addr, Index size, Version* versions, uint32_t width);
	void set(Addr addr, Index size, Version* versions, Time* times, uint32_t width);
};

#endif

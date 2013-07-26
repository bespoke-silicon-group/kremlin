#ifndef _MSHADOW_H
#define _MSHADOW_H

#include "ktypes.h"

class MShadow {
public:
	virtual void init() = 0;
	virtual void deinit() = 0;

	virtual Time* get(Addr addr, Index size, Version* versions, UInt32 width) = 0;
	virtual void set(Addr addr, Index size, Version* versions, Time* times, UInt32 width) = 0;
};
#endif

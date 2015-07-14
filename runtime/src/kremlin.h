#ifndef _KREMLIN_H
#define _KREMLIN_H
#define NDEBUG
//#define KREMLIN_DEBUG

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "ktypes.h"
#include "debug.hpp"
#include "MemMapAllocator.hpp"


extern Time* (*MShadowGet)(Addr, Index, Version*, uint32_t);
extern void  (*MShadowSet)(Addr, Index, Version*, Time*, uint32_t) ;


Level getMaxActiveLevel();

#endif

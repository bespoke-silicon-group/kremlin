#ifndef _KREMLIN_H
#define _KREMLIN_H
//#define NDEBUG
//#define KREMLIN_DEBUG

#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include "ktypes.h"
#include "debug.h"
#include "MemMapAllocator.h"



extern Time* (*MShadowGet)(Addr, Index, Version*, UInt32);
extern void  (*MShadowSet)(Addr, Index, Version*, Time*, UInt32) ;


// utility functions
extern Level __kremlin_min_level;
extern Level __kremlin_max_level;

#define getMinLevel() (__kremlin_min_level)
#define getMaxLevel() (__kremlin_max_level)
#define getIndexSize() (__kremlin_max_level - __kremlin_min_level + 1)
#define getLevel(index) ((index) + __kremlin_min_level)


#endif

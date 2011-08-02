#ifndef _MSHADOW_H
#define _MSHADOW_H


/*
 * Memory Shadow Interface
 *
 * djeon@cs.ucsd.edu
 */


#include "defs.h"
#include "TEntry.h"


UInt 		MShadowInit();			// initialize global shadow memory system
UInt 		MShadowFinalize();		// free associated data structure

Timestamp	MShadowGetTimestamp(Addr addr, Index index);
void   		MShadowSetTimestamp(Timestamp time, Addr addr, Index index);


#endif

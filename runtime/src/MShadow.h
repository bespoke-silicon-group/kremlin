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
UInt 		MShadowDeinit();		// free associated data structure

//Time		MShadowGet(Addr addr, Index index, Version version);
//void   		MShadowSet(Addr addr, Index index, Version version, Time time);

void		MShadowGet(Addr addr, Index size, Version* vArray, Time* tArray);
void		MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray);


#endif

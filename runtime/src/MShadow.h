#ifndef _MSHADOW_H
#define _MSHADOW_H

/*
 * Memory Shadow Interface
 *
 * djeon@cs.ucsd.edu
 */


#include "defs.h"


//UInt 		MShadowInit(int, int);	// initialize global shadow memory system
//UInt 		MShadowDeinit();		// free associated data structure

Time		MShadowGetTime(Addr addr, Index size, Version version);
void		MShadowSetTime(Addr addr, Index size, Version version, Time time);
//Time*		MShadowGet(Addr addr, Index size, Version* vArray, UInt32 width);
//void		MShadowSet(Addr addr, Index size, Version* vArray, Time* tArray, UInt32 width);




#endif

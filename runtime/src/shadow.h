#ifndef _GSHADOW_H
#define _GSHADOW_H


/*
 * Global Shadow Memory Interface
 *
 * djeon@cs.ucsd.edu
 */


#include "defs.h"


UInt 		memShadowInit();			// initialize global shadow memory system
UInt 		memShadowFinalize();		// free associated data structure

Timestamp	memGetTimestamp(Addr addr, Level level);
void   		memSetTimestamp(Timestamp time, Addr addr, Level level);


UInt 		regShadowInit();			// initialize global shadow memory system
UInt 		regShadowFinalize();		// free associated data structure

Timestamp	regGetTimestamp(Reg reg, Level level);
void   		regSetTimestamp(Timestamp time, Reg reg, Level level);



#endif

#ifndef _MSHADOW_H
#define _MSHADOW_H


/*
 * Global Shadow Memory Interface
 *
 * djeon@cs.ucsd.edu
 */


#include "defs.h"

#include "TEntry.h"

typedef struct GTable GTable;

int GTableCreate(GTable** t);
int GTableDelete(GTable** t);
TEntry* GTableGetTEntry(GTable* t, Addr addr);
int GTableDeleteTEntry(GTable* t, Addr addr);



UInt 		memShadowInit();			// initialize global shadow memory system
UInt 		memShadowFinalize();		// free associated data structure

Timestamp	memGetTimestamp(Addr addr, Level level);
void   		memSetTimestamp(Timestamp time, Addr addr, Level level);




#endif

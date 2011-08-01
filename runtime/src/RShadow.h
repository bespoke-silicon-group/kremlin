#ifndef _RSHADOW_H
#define _RSHADOW_H


/*
 * Global Shadow Memory Interface
 *
 * djeon@cs.ucsd.edu
 */


#include "defs.h"
#include "TEntry.h"

typedef struct _LocalTable {
	int          size;
    TEntry**     array;
} LTable;

UInt 		RShadowInit();			// initialize global shadow memory system
UInt 		RShadowFinalize();		// free associated data structure

LTable*		RShadowCreateTable(int numEntry, Index depth);
void		RShadowFreeTable(LTable* table);

Timestamp	RShadowGetTimestamp(Reg reg, Index index);
void   		RShadowSetTimestamp(Timestamp time, Reg reg, Index index);
void		RShadowShadowToArg(Arg* dest, Reg src);
void		RShadowArgToShadow(Reg dest, Arg* src);

void 		RShadowSetActiveLTable(LTable* table);


#endif

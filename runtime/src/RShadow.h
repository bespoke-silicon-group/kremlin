#ifndef _RSHADOW_H
#define _RSHADOW_H


/*
 * Global Shadow Memory Interface
 *
 * djeon@cs.ucsd.edu
 */


#include "defs.h"
#include "TEntry.h"
#include "TArray.h"

typedef struct _LocalTable {
	int          size;
    TEntry**     array;
} LTable;

UInt 		RShadowInit();			// initialize global shadow memory system
UInt 		RShadowDeinit();		// free associated data structure

LTable*		RShadowCreateTable(int numEntry, Index depth);
void		RShadowFreeTable(LTable* table);

Time		RShadowGet(Reg reg, Index index);
void   		RShadowSet(Time time, Reg reg, Index index);
void		RShadowExport(TArray* dest, Reg src);
void		RShadowImport(Reg dest, TArray* src);

void 		RShadowActiveTable(LTable* table);


#endif

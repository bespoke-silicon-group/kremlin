#ifndef _RSHADOW_H
#define _RSHADOW_H


/*
 * Global Shadow Memory Interface
 *
 * djeon@cs.ucsd.edu
 */


#include "defs.h"
#include "Table.h"

typedef struct _LocalTable {
	int         entrySize;
	int         indexSize;
    Time*		array;
	UInt		code;
} LTable;

UInt 		RShadowInit();			// initialize global shadow memory system
UInt 		RShadowDeinit();		// free associated data structure

LTable*		RShadowCreateTable(int numEntry, Index depth);
void		RShadowFreeTable(LTable* table);

Time		RShadowGet(Reg reg, Index index);
void   		RShadowSet(Time time, Reg reg, Index index);

void		RShadowCopy(LTable* destTable, Reg destReg, LTable* srcTable, Reg srcReg, 
				Index start, Index size);
Time		RShadowGetWithTable(LTable* table, Reg reg, Index index);
void		RShadowSetWithTable(LTable* table, Time time, Reg reg, Index index);
void 		RShadowActiveTable(LTable* table);


#endif

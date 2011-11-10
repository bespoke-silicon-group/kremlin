#ifndef _RSHADOW_H
#define _RSHADOW_H


/*
 * Global Shadow Memory Interface
 *
 * djeon@cs.ucsd.edu
 */


#include "ktypes.h"
#include "Table.h"

#if 0
typedef struct _LocalTable {
	int         entrySize;
	int         indexSize;
    Time*		array;
	UInt		code;
} LTable;
#endif

UInt 		RShadowInit();			// initialize global shadow memory system
UInt 		RShadowDeinit();		// free associated data structure

Table*		RShadowCreateTable(int numEntry, Index depth);
void		RShadowFreeTable(Table* table);

Time		RShadowGetItem(Reg reg, Index index);
void   		RShadowSetItem(Time time, Reg reg, Index index);
Time*		RShadowGet(Reg reg, Index size);
void   		RShadowSet(Reg reg, Index size, Time* tArray);

void		RShadowCopy(Table* destTable, Reg destReg, Table* srcTable, Reg srcReg, 
				Index start, Index size);
Time		RShadowGetWithTable(Table* table, Reg reg, Index index);
void		RShadowSetWithTable(Table* table, Time time, Reg reg, Index index);
void 		RShadowActiveTable(Table* table);


#endif

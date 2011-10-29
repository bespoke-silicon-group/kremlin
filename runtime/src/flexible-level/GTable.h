#ifndef GTABLE_H
#define GTABLE_H

#include "table.h"


typedef struct GTableL2 {
	TSEntry* array[L2_SIZE];
} GTableL2;

typedef struct GTableL1 {
	GTableL2* array[L1_SIZE];
} GTableL1;

typedef struct GTableL0 {
	GTableL1* array[L0_SIZE];
} GTableL0;

typedef struct LevelTable {
	GTableL1* levels;
	UInt num_levels;
} LevelTable;

typedef struct GlobalSM {
	LevelTable lt;
} GlobalSM;

void getTSArrayGlobal(GlobalSM* gsm, Addr src_addr, TSArray* dest_array);

// BEGIN OLD STUFF

typedef struct GTable GTable;

int GTableCreate(GTable** t);
int GTableDelete(GTable** t);
TEntry* GTableGetTEntry(GTable* t, Addr addr);
int GTableDeleteTEntry(GTable* t, Addr addr);

#endif /* GTABLE_H */

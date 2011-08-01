#ifndef GTABLE_H
#define GTABLE_H

#include "tentry.h"

typedef struct GTable GTable;

int GTableCreate(GTable** t);
int GTableDelete(GTable** t);
TEntry* GTableGetTEntry(GTable* t, Addr addr);
int GTableDeleteTEntry(GTable* t, Addr addr);

#endif /* GTABLE_H */

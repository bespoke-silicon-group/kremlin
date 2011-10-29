// TODO: reference count pages properly and free them when no longer used.

#include "GTable.h"
#include "table.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

TSArray* getTSArrayGlobal(Addr addr);

// BEGIN OLD STUFF

// Mask of the L1 table.
#define L1_MASK 0xffff

// Size of the L1 table.
#define L1_SIZE (L1_MASK + 1)

// Bits to shift right before the mask
#define L1_SHIFT 16

// Mask of the L2 table.
#define L2_MASK 0x3fff

// The size of the L2 table. Since only word addressible, shift by 2
#define L2_SIZE (L2_MASK + 1)

#define L2_SHIFT 2

/*
 * Typedefs
 */
typedef struct GTEntry GTEntry;

/*
 * Prototypes
 */
static int GTEntryCreate(GTEntry** e);
static int GTEntryDelete(GTEntry** e);
static int GTEntryDeleteTEntry(GTEntry*, Addr addr);
static TEntry** GTEntryGet(GTEntry* e, Addr addr);
static GTEntry** GTableGetGTEntry(GTable* t, Addr addr);
static UInt64 GTableIndex(Addr addr);
static UInt64 GTEntryIndex(Addr addr);

/*
 * Struct definitions
 */
struct GTEntry {
    unsigned short used; // number of entries that are in use
    unsigned short usedLine; // number of entries that are in use
    TEntry* array[L2_SIZE];
    TEntry* lineArray[L2_SIZE >> CACHE_LINE_POWER_2];
};

/*  
    GlobalTable:
        global table is a hashtable with lower address as its primary key.
*/
struct GTable {
    GTEntry* array[L1_SIZE];
};

int GTEntryCreate(GTEntry** e)
{
    if(!(*e = (GTEntry*)calloc(sizeof(GTEntry), 1)))
    {
        assert(0 && "calloc returned NULL in GTEntryCreate");
        return FALSE;
    }
    return TRUE;
}

int GTEntryDelete(GTEntry** e)
{
    int i;
    for(i = 0; i < L2_SIZE; i++)
    {
        TEntry* tEntry = (*e)->array[i];
        if(tEntry)
            freeTEntry(tEntry);
    }

    free(*e);
    *e = NULL;
}

int GTEntryDeleteTEntry(GTEntry* e, Addr addr)
{
    TEntry** tEntry = GTEntryGet(e, addr);
    freeTEntry(*tEntry);
    *tEntry = NULL;

    e->used--;
}

TEntry** GTEntryGet(GTEntry* e, Addr addr)
{
    TEntry** tEntry = e->array + GTEntryIndex(addr);
    if(!*tEntry)
    {
        *tEntry = allocTEntry();
        e->used++;
		e->usedLine += 1;
		// _tEntryGlobalCnt++;
    }
    return tEntry;
}

UInt64 GTEntryIndex(Addr addr)
{
    return ((UInt64)addr >> L2_SHIFT) & L2_MASK;
}

int GTableCreate(GTable** t)
{
    if(!(*t = (GTable*)calloc(1, sizeof(GTable))))
    {
        assert(0 && "calloc failed");
        return FALSE;
    }
    return TRUE;
}

int GTableDelete(GTable** t) {
	int i, j;
	for (i = 0; i < L1_SIZE; i++) {
		if ((*t)->array[i] != NULL) {
            GTEntryDelete((*t)->array + i);
		}
	}
	free(*t);
    *t = NULL;
    return TRUE;
}

// get TEntry for address addr in GTable t
// FIXME: 64bit address?
TEntry* GTableGetTEntry(GTable* t, Addr addr) 
{
#ifndef WORK_ONLY
    GTEntry** entry = GTableGetGTEntry(t, addr);
    return *GTEntryGet(*entry, addr);
#else
	return (TEntry*)1;
#endif
}

// get GTEntry in GTable t at address addr
GTEntry** GTableGetGTEntry(GTable* t, Addr addr)
{
    UInt32 index = GTableIndex(addr);
	GTEntry** entry = t->array + index;
	if(*entry == NULL)
		GTEntryCreate(entry);
    return entry;
}

int GTableDeleteTEntry(GTable* t, Addr addr)
{
    GTEntry** entry = GTableGetGTEntry(t, addr);
    GTEntryDeleteTEntry(*entry, addr);
    return TRUE;
}

UInt64 GTableIndex(Addr addr)
{
	return ((UInt64)addr >> L1_SHIFT) & L1_MASK;
}

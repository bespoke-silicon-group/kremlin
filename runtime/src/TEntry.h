#ifndef TENTRY_H
#define TENTRY_H

#include "defs.h"


typedef struct _DataEntry {
    Version* 	version;
    Timestamp* 	time;
    Index		depth;

#ifdef EXTRA_STATS
    UInt32* readVersion;
    UInt64* readTime;
#endif

} TEntry;

// declaration of functions in table.c
TEntry* 	TEntryAlloc(Index depth);
void 		TEntryFree(TEntry* entry);

Timestamp 	TEntryGet(TEntry* entry, Index index, Version version);
void 		TEntryUpdate(TEntry* entry, Index index, Version version, Timestamp timestamp);

void 		TEntryCopy(TEntry* dest, TEntry* src);
void 		TEntryRealloc(TEntry* entry, Index depth);

void		TEntryDump(TEntry* entry, int size);


/*
#if KREMLIN_DEBUG == 1
	char* toStringTEntry(TEntry* entry);
#else
#	define toStringTEntry(entry)       ((void)0)
#endif
*/

#endif /* TENTRY_H */

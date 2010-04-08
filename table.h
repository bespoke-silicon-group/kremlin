#ifndef _TABLE_H
#define _TABLE_H

// declaration of functions in table.c
void setLocalTable(LTable* table);
UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version);
UInt64 getTimestampNoVersion(TEntry* entry, UInt32 level);
void copyTEntry(TEntry* dest, TEntry* src);
UInt32 getMaxRegionLevel();
void finalizeDataStructure();


#endif

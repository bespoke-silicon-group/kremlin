#ifndef _TABLE_H
#define _TABLE_H

// declaration of functions in table.c
void setLocalTable(LTable* table);
UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version);
UInt64 getTimestampNoVersion(TEntry* entry, UInt32 level);
void copyTEntry(TEntry* dest, TEntry* src);
UInt32 getMaxRegionLevel();
void finalizeDataStructure();

GEntry* createGEntry();
void createMEntry(Addr start_addr, size_t entry_size);
MEntry* getMEntry(Addr start_addr);
void freeMEntry(Addr start_addr);

GTable* gTable;
LTable* lTable;
MTable* mTable;
UInt32	maxRegionLevel;
#endif

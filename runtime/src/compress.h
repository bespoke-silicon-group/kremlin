#ifndef _COMPRESS_H
#define _COMPRESS_H

void printTArray(Time* tArray);
void printTimeTable(TimeTable* tTable);

UInt8 tArrayIsDiff(Time *array1, Time *array2);
UInt64 compressLTable(LTable* lTable, Version* vArray);
UInt64 decompressLTable(LTable* lTable);
UInt64 compressShadowMemory(Version* vArray);

#endif

#ifndef _COMPRESS_H
#define _COMPRESS_H

// COMPRESSION_POLICY options:
//	0: no compression
//	1: compress on every shadow memory store
//	2: compress when threshold is reached
#define COMPRESSION_POLICY 1

// this is ignored if COMPRESSION_POLICY != 1
#define COMPRESSION_THRESHOLD 10000000

// says that we should do garbage collection before decompressing
#define GC_BEFORE_DECOMPRESS

// 0: array
// 1: hash map
#define ACTIVE_SET_IMPLEMENTATION 0

// this is ignored if COMPRESSION POLICY != 0
#if ACTIVE_SET_IMPLEMENTATION == 0
#define UNCOMPRESSED_BUFFER_SIZE 16
#elif ACTIVE_SET_IMPLEMENTATION == 1
#define UNCOMPRESSED_BUFFER_SIZE 1024
#endif

void printTArray(Time* tArray);
void printTimeTable(TimeTable* tTable);

UInt8 tArrayIsDiff(Time *array1, Time *array2);
UInt64 compressLTable(LTable* lTable);
UInt64 decompressLTable(LTable* lTable);
UInt64 compressShadowMemory(Version* vArray);
UInt64 calculateTimeTableOverhead();

#endif

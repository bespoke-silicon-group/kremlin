#ifndef _MSHADOW_LOW
#define _MSHADOW_LOW

#include "ktypes.h"

// TimeTable Parameters
#define TIMETABLE_MASK 0x3ff	// 4KB
#define TIMETABLE_SIZE (TIMETABLE_MASK + 1)

typedef struct _TimeTable {
	UInt8  type;	// 32BIT or 64BIT
	UInt32 size;	// can be very small when compressed
	Time*  array;	// actual Timestamp data
} TimeTable;


#define MAX_LEVEL	64
//TODO: need to support dynamically growing / shrinking without MAX_LEVEL
typedef struct _LTable {
	UInt32		code;
	UInt8		isCompressed; 		// 0 = uncompressed, 1 = compressed
	Version		vArray[MAX_LEVEL];	// version for each level
	TimeTable* 	tArray[MAX_LEVEL];	// TimeTable for each level
} LTable;


/*
 * SegTable: Covers a 4GB space 
 */
#define SEGTABLE_MASK 	0xfffff	
#define SEGTABLE_SHIFT	12
#define SEGTABLE_SIZE 	(SEGTABLE_MASK + 1)

#define TYPE_64BIT	0
#define TYPE_32BIT	1


typedef struct _Segment {
	LTable* entry[SEGTABLE_SIZE];
} SegTable;


void SkaduEvict(Time* tArray, Addr addr, int size, Version* vArray, int type);
void SkaduFetch(Addr addr, Index size, Version* vArray, Time* destAddr, int type);
#endif

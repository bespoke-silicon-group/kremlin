#include "defs.h"

#define MAX_LEVEL	64

// SEGTABLE Parameters
#define SEGTABLE_MASK 	0xfffff
#define SEGTABLE_SHIFT	12
#define SEGTABLE_SIZE 	(SEGTABLE_MASK + 1)

// TimeTable Parameters
#define TIMETABLE_MASK 0x3ff
#define TIMETABLE_SIZE (TIMETABLE_MASK + 1)

#define TYPE_64BIT	0
#define TYPE_32BIT	1

typedef struct _TimeTable {
	UInt8 type;
	UInt8 useVersion;
	Time* array;
	Version* version;
} TimeTable;

typedef struct _LTable {
	UInt8		isCompressed; // 0 = uncompressed, 1 = compressed
	UInt8		noBTV[MAX_LEVEL];
	UInt32		compressedLen[MAX_LEVEL];
	UInt64		nAccess[MAX_LEVEL];
	Version		vArray[MAX_LEVEL];
	TimeTable* 	tArray[MAX_LEVEL];
	//Time* 		tArrayBackup[MAX_LEVEL];
} LTable;


typedef struct _Segment {
	LTable* entry[SEGTABLE_SIZE];
} SegTable;

/*
 * STable: sparse table that tracks 4GB memory chunks being used
 *
 * Since 64bit address is very sparsely used in a program,
 * we use a sparse table to reduce the memory requirement of the table.
 * Although the walk-through of a table might be pricey,
 * the use of cache will make the frequency of walk-through very low.
 */

typedef struct _SEntry {
	UInt32 	addrHigh;	// upper 32bit in 64bit addr
	SegTable* segTable;
} SEntry;

#define STABLE_SIZE		32		// 128GB addr space will be enough

typedef struct _STable {
	SEntry entry[STABLE_SIZE];	
	int writePtr;
} STable;


STable sTable;

//void gcLevel(LTable* table, Version* vArray);
//TimeTable* TimeTableAlloc(int sizeType, int useVersion);

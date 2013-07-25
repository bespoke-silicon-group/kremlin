#ifndef _MSHADOW_LOW
#define _MSHADOW_LOW

#include "ktypes.h"

class TimeTable {
public:
	enum TableType { TYPE_64BIT, TYPE_32BIT };

	static const unsigned TIMETABLE_MASK = 0x3ff;
	static const int TIMETABLE_SIZE = TIMETABLE_MASK+1;

	TableType type;
	UInt32 size;	// can be very small when compressed
	Time*  array;	// actual Timestamp data

	/*! \brief Zero out all time table entries. */
	void clean() {
		int size = TimeTable::GetEntrySize(type);
		memset(array, 0, sizeof(Time) * size);
	}


	/*!
	 * \remark Not affected by access width
	 */
	Time getTimeAtAddr(Addr addr) {
		int index = TimeTable::GetIndex(addr, type);
		Time ret = array[index];
		return ret;
	}

	void setTimeAtAddr(Addr addr, Time time, TableType type);

	static TimeTable* Create(TableType size_type);
	static void Destroy(TimeTable* table, UInt8 isCompressed);
	static TimeTable* Create32BitClone(TimeTable* table);

	static int GetEntrySize(TableType type) {
		int size = TimeTable::TIMETABLE_SIZE;
		if (type == TYPE_64BIT) size >>= 1;
		return size;
	}

	static int GetIndex(Addr addr, TableType type) {
		const int WORD_SHIFT = 2;
		int ret = ((UInt64)addr >> WORD_SHIFT) & TimeTable::TIMETABLE_MASK;
		assert(ret < TimeTable::TIMETABLE_SIZE);
		if (type == TYPE_64BIT) ret >>= 1;

		return ret;
	}
};


//TODO: need to support dynamically growing / shrinking without MAX_LEVEL
class LevelTable {
public:
	static const unsigned MAX_LEVEL = 64;

	UInt32		code;
	UInt8		isCompressed; 		// 0 = uncompressed, 1 = compressed
	Version		vArray[LevelTable::MAX_LEVEL];	// version for each level
	TimeTable* 	tArray[LevelTable::MAX_LEVEL];	// TimeTable for each level

	static LevelTable* Alloc() {
		LevelTable* ret = (LevelTable*)MemPoolCallocSmall(1, sizeof(LevelTable));
		ret->code = 0xDEADBEEF;
		return ret;
	}

	Version getVersionAtLevel(Index level) { return vArray[level]; }
	void setVersionAtLevel(Index level, Version ver) {
		vArray[level] = ver;
	}

	TimeTable* getTimeTableAtLevel(Index level) {
		return tArray[level];
	}
	void setTimeTableAtLevel(Index level, TimeTable* table) {
		tArray[level] = table;
	}

	Time getTimeForAddrAtLevel(Index level, Addr addr, Version curr_ver);
	void setTimeForAddrAtLevel(Index level, Addr addr, Version curr_ver, Time value, TimeTable::TableType type);

	int findLowestInvalidIndex(Version* vArray);
	void cleanTimeTablesFromLevel(Index start_level);
};


/*
 * SegTable: Covers a 4GB space 
 */
class SegTable {
private:
	static const unsigned SEGTABLE_MASK = 0xfffff;
	static const unsigned SEGTABLE_SHIFT = 12;

public:
	static const unsigned SEGTABLE_SIZE = SEGTABLE_MASK+1;

	LevelTable* entry[SEGTABLE_SIZE];

	static SegTable* Alloc() {
		SegTable* ret = (SegTable*)MemPoolCallocSmall(1,sizeof(SegTable));
		return ret;	
	}

	static void Free(SegTable* table) {
		MemPoolFreeSmall(table, sizeof(SegTable));
	}

	static int GetIndex(Addr addr) {
		return ((UInt64)addr >> SEGTABLE_SHIFT) & SEGTABLE_MASK;
	}
};


void SkaduEvict(Time* tArray, Addr addr, int size, Version* vArray, TimeTable::TableType type);
void SkaduFetch(Addr addr, Index size, Version* vArray, Time* destAddr, TimeTable::TableType type);
#endif

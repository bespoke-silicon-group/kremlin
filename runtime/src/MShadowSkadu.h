#ifndef _MSHADOW_SKADU_H
#define _MSHADOW_SKADU_H

#include "ktypes.h"
#include "MShadow.h"

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

	static int GetIndex(Addr addr, TableType type);
};


//TODO: need to support dynamically growing / shrinking without MAX_LEVEL
class LevelTable {
public:
	static const unsigned MAX_LEVEL = 64;

	UInt32		code;
	UInt8		isCompressed; 		// 0 = uncompressed, 1 = compressed
	Version		vArray[LevelTable::MAX_LEVEL];	// version for each level
	TimeTable* 	tArray[LevelTable::MAX_LEVEL];	// TimeTable for each level

	static LevelTable* Alloc();

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
	void gcLevel(Version* versions, int size);
	void gcLevelUnknownSize(Version* versions);
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

	static SegTable* Alloc();
	static void Free(SegTable* table);

	static int GetIndex(Addr addr) {
		return ((UInt64)addr >> SEGTABLE_SHIFT) & SEGTABLE_MASK;
	}
};

class MShadowSkadu;

class CacheInterface {
protected:
	bool use_compression;
	MShadowSkadu *mem_shadow;

public:
	virtual void init(int size, bool compress, MShadowSkadu* mshadow) = 0;
	virtual void deinit() = 0;

	virtual void set(Addr addr, Index size, Version* vArray, Time* tArray, TimeTable::TableType type) = 0;
	virtual Time* get(Addr addr, Index size, Version* vArray, TimeTable::TableType type) = 0;
};

class SparseTable;

class MShadowSkadu : public MShadow {
private:
	SparseTable* sparse_table; 

	UInt64 next_gc_time;
	int gc_period;

	void initGarbageCollector(int period);
	void runGarbageCollector(Version* versions, int size);

	CacheInterface *cache;

public:
	void init();
	void deinit();

	Time* get(Addr addr, Index size, Version* versions, UInt32 width);
	void set(Addr addr, Index size, Version* versions, Time* times, UInt32 width);

	void fetch(Addr addr, Index size, Version* vArray, Time* destAddr, TimeTable::TableType type);
	void evict(Time* tArray, Addr addr, int size, Version* vArray, TimeTable::TableType type);

	LevelTable* getLevelTable(Addr addr, Version* vArray);
};

#endif

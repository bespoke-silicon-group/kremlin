#ifndef _MSHADOW_SKADU_H
#define _MSHADOW_SKADU_H

#include "ktypes.h"
#include "MShadow.h"

/*!
 * A simple array of Time with TIMETABLE_SIZE elements
 */ 
class TimeTable {
public:
	enum TableType { TYPE_64BIT, TYPE_32BIT };

	static const unsigned TIMETABLE_MASK = 0x3ff;
	static const unsigned TIMETABLE_SIZE = TIMETABLE_MASK+1;

	TableType type; //!< The access type of this table (32 or 64-bit)
	UInt32 size;	//!< The number of bytes in array
					// XXX: is size member variable necessary?
	Time* array;	//!< The timestamp data array.

	/*!
	 * Constructor to create Timetable of the given type.
	 *
	 * @param size_type Type of the table (64-bit or 32-bit)
	 * @post TimeTable array will be non-NULL.
	 */
	TimeTable(TableType size_type);
	~TimeTable();

	/*! 
	 * @brief Zero out all time table entries.
	 */
	void clean() {
		unsigned size = TimeTable::GetNumEntries(type);
		memset(array, 0, sizeof(Time) * size);
	}

	/*!
	 * Get the timestamp associated with the given address.
	 * @remark Not affected by access width
	 *
	 * @param addr The address whose timestamp to return.
	 * @return The timestamp of the specified address.
	 * @pre addr is non-NULL
	 */
	Time getTimeAtAddr(Addr addr) {
		assert(addr != NULL);
		unsigned index = this->getIndex(addr);
		Time ret = array[index];
		return ret;
	}

	/*!
	 * Sets the time associated with the specified address.
	 *
	 * @param addr The address whose time should be set.
	 * @param time The value to which we will set the time.
	 * @param access_type Type of access (32 or 64-bit)
	 * @pre addr is non-NULL
	 */
	void setTimeAtAddr(Addr addr, Time time, TableType access_type);

	/*!
	 * @brief Construct a 32-bit version of this 64-bit TimeTable.
	 *
	 * @return Pointer to a new TimeTable that is the 32-bit equivalent of
	 * this 64-bit table.
	 * @pre This TimeTable is 64-bit.
	 */
	TimeTable* create32BitClone();

	/*!
	 * Returns the number of entries in the timetable array for a TimeTable
	 * with the specified type.
	 *
	 * @param type The type of the TimeTable.
	 * @return The number of entries in the timestamp array.
	 */
	static unsigned GetNumEntries(TableType type) {
		unsigned size = TimeTable::TIMETABLE_SIZE;
		if (type == TYPE_64BIT) size >>= 1;
		return size;
	}

	/*!
	 * Returns index into timetable array for the given address.
	 *
	 * @param addr The address for which to get the index.
	 * @return The index of the timetable array associated with the addr/type.
	 * @pre addr is non-NULL
	 * @post The index returned is less than TIMETABLE_SIZE (for 32-bit) or
	 * TIMETABLE_SIZE/2 (for 64-bit)
	 */
	unsigned getIndex(Addr addr);

	static void* operator new(size_t size);
	static void operator delete(void* ptr);
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

	/*! @brief Compress the level table.
	 *
	 * @remark It is assumed you already garbage collected the table, otherwise
	 * you are going to be compressing out of data data.
	 * @return The number of bytes saved by compression.
	 * @pre This LevelTable is not compressed.
	 * @post isCompressed is 1.
	 * @invariant code is 0xDEADBEEF
	 */
	UInt64 compress();

	/*! @brief Decompress the level table.
	 *
	 * @return The number of bytes lost by decompression.
	 * @pre This LevelTable is compressed.
	 * @invariant code is 0xDEADBEEF
	 */
	UInt64 decompress();

private:
	/*! @brief Modify array so elements are difference between that element 
	 * and the previous element.
	 *
	 * @param[in,out] array The array to convert
	 * @pre array is non-NULL.
	 */
	void makeDiff(Time *array);

	/*! @brief Perform inverse operation of makeDiff
	 *
	 * @param[in,out] array The array to convert.
	 * @pre array is non-NULL.
	 */
	void restoreDiff(Time *array);
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
class CBuffer;

class MShadowSkadu : public MShadow {
private:
	SparseTable* sparse_table; 

	UInt64 next_gc_time;
	int gc_period;

	void initGarbageCollector(int period);
	void runGarbageCollector(Version* versions, int size);

	CacheInterface *cache;
	CBuffer* compression_buffer;

public:
	void init();
	void deinit();

	Time* get(Addr addr, Index size, Version* versions, UInt32 width);
	void set(Addr addr, Index size, Version* versions, Time* times, UInt32 width);

	void fetch(Addr addr, Index size, Version* vArray, Time* destAddr, TimeTable::TableType type);
	void evict(Time* tArray, Addr addr, int size, Version* vArray, TimeTable::TableType type);

	LevelTable* getLevelTable(Addr addr, Version* vArray);
	CBuffer* getCompressionBuffer() { return compression_buffer; }
};

#endif

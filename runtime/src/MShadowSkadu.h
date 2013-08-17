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
	 */
	Time getTimeAtAddr(Addr addr) {
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
	 * @post The index returned is less than TIMETABLE_SIZE (for 32-bit) or
	 * TIMETABLE_SIZE/2 (for 64-bit)
	 */
	unsigned getIndex(Addr addr);

	static void* operator new(size_t size);
	static void operator delete(void* ptr);
};


//TODO: need to support dynamically growing / shrinking without MAX_LEVEL
class LevelTable {
private:
	static const unsigned MAX_LEVEL = 64;
	Version	versions[LevelTable::MAX_LEVEL];	//!< version for each level
	TimeTable* time_tables[LevelTable::MAX_LEVEL];	//!< TimeTable for each level

public:
	UInt32 code;
	UInt8 isCompressed; 		// 0 = uncompressed, 1 = compressed

	/*!
	 * Default, no-argument constructor. Sets all versions to 0 and all
	 * time_tables to NULL.
	 */
	LevelTable();

	~LevelTable();

	/*!
	 * Returns version at specified level.
	 *
	 * @param level The level at which to get the version.
	 * @pre level < MAX_LEVEL
	 */
	Version getVersionAtLevel(Index level) {
		assert(level < LevelTable::MAX_LEVEL);
		return versions[level];
	}

	/*!
	 * Sets version at specified level to a given value.
	 *
	 * @param level The level at which to get the version.
	 * @param ver The version we will set it to.
	 * @pre level < MAX_LEVEL
	 */
	void setVersionAtLevel(Index level, Version ver) {
		assert(level < LevelTable::MAX_LEVEL);
		versions[level] = ver;
	}

	/*!
	 * Returns pointer to TimeTable at specified level.
	 * @remark The returned pointer may be NULL.
	 *
	 * @param level The level at which to get the TimeTable.
	 * @pre level < MAX_LEVEL
	 */
	TimeTable* getTimeTableAtLevel(Index level) {
		assert(level < LevelTable::MAX_LEVEL);
		TimeTable* t = time_tables[level];
		return t;
	}

	/*!
	 * Sets TimeTable at specified level to a given value.
	 *
	 * @param level The level at which to get the version.
	 * @param table New value for TimeTable* at level.
	 * @pre level < MAX_LEVEL
	 * @pre table is non-NULL
	 */
	void setTimeTableAtLevel(Index level, TimeTable *table) {
		assert(level < LevelTable::MAX_LEVEL);
		assert(table != NULL);
		time_tables[level] = table;
	}

	/*!
	 * Returns timestamp associated with a given address and level. The
	 * returned timestamp will be 0 either if there is no entry for the given
	 * address and level or if the existing entry is out of date (i.e. the
	 * stored version number differs from the given version number).
	 *
	 * @param level The level at which to get the TimeTable.
	 * @param addr The address in shadow memory whose timestamp we want.
	 * @param curr_ver The current version value.
	 * @pre level < MAX_LEVEL
	 */
	Time getTimeForAddrAtLevel(Index level, Addr addr, Version curr_ver);

	/*!
	 * Sets timestamp associated with a given address and level to a specified
	 * value.
	 *
	 * @param level The level at which to get the TimeTable.
	 * @param addr The address in shadow memory whose timestamp we want.
	 * @param curr_ver The current version value.
	 * @param value The new time to set it to.
	 * @param type The type of access (32 or 64-bit)
	 * @pre level < MAX_LEVEL
	 */
	void setTimeForAddrAtLevel(Index level, Addr addr, 
								Version curr_ver, Time value, 
								TimeTable::TableType type);

	/*!
	 * @brief Returns the shallowest depth at which the level table is invalid.
	 *
	 * A given depth is invalid if any of these conditions are met:
	 * 1. The depth exceeds the MAX_LEVEL for the LevelTable.
	 * 2. The timestamp* at that depth is NULL.
	 * 3. The stored version (versions) at that depth is less than the version at
	 * that depth in the version array parameter.
	 *
	 * @param curr_versions Array of versions with which to compare.
	 * @pre curr_versions is non-NULL.
	 */
	unsigned findLowestInvalidIndex(Version *curr_versions);

	/*!
	 * @brief Removed all TimeTables from the given depth down to MAX_LEVEL.
	 *
	 * @param start_level The level to start the cleaning.
	 */
	void cleanTimeTablesFromLevel(Index start_level);

	/*!
	 * @brief Performs garbage collection on the TimeTables in this level table
	 * up to the specified depth. All times below that depth will be cleared.
	 *
	 * When a level is garbage collected, the corresponding TimeTable is
	 * deleted and the pointer to it set to NULL. Garbage collection occurs
	 * in a level whenever the stored version for that level is less than the
	 * current version for that level.
	 *
	 * @param curr_versions The array of current versions.
	 * @param end_index The maximum level to garbage collect for.
	 * @pre curr_versions is non-NULL.
	 * @pre end_index < MAX_LEVEL
	 */
	void collectGarbageWithinBounds(Version *curr_versions, unsigned end_index);

	/*!
	 * @brief Removes all "garbage" TimeTables in this LevelTable.
	 *
	 * A TimeTable is considered garbage if its associated depth has an
	 * outdated version, i.e. the stored version is less than the
	 * corresponding version in the current version array.
	 *
	 * @param curr_versions The array of current version numbers.
	 * @pre curr_versions is non-NULL.
	 */
	void collectGarbageUnbounded(Version *curr_versions);

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

	static void* operator new(size_t size);
	static void operator delete(void* ptr);

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

	/*! @brief Get the depth of this level table (i.e. how many valid
	 * TimeTables it has)
	 *
	 * @return Number of entries in specified level table.
	 * @pre At least one TimeTable* in this level table is NULL.
	 */
	unsigned getDepth();
};


/*!
 * Manages 4GB of consecutive memory space.
 */
class MemorySegment {
private:
	static const unsigned SEGMENT_MASK = 0xfffff;
	static const unsigned SEGMENT_SHIFT = 12;
	static const unsigned NUM_ENTRIES = SEGMENT_MASK+1;

	LevelTable* level_tables[NUM_ENTRIES]; //!< The LevelTables associated 
												// with this MemorySegment

public:
	/*!
	 * Default constructor. Sets all LevelTable* in level_tables to NULL.
	 */
	MemorySegment();

	/*!
	 * Destructor. Deletes any valid (i.e non-NULL) LevelTables pointed to by
	 * this MemorySegment.
	 */
	~MemorySegment();

	/*!
	 * Return the LevelTable* at the specified index in this MemorySegment.
	 *
	 * @param index The index of the LevelTable* to return.
	 * @pre index < NUM_ENTRIES
	 */
	LevelTable* getLevelTableAtIndex(unsigned index) { 
		assert(index < NUM_ENTRIES);
		return level_tables[index];
	}

	/*!
	 * Sets the LevelTable* at the specified index in this MemorySegment.
	 *
	 * @param table The LevelTable* to which we will set it.
	 * @param index The index of the LevelTable* to set.
	 * @pre table is non-NULL.
	 * @pre index < NUM_ENTRIES
	 */
	LevelTable* setLevelTableAtIndex(LevelTable *table, unsigned index) { 
		assert(table != NULL);
		assert(index < NUM_ENTRIES);
		level_tables[index] = table;
	}

	static unsigned getNumLevelTables() { return NUM_ENTRIES; }
	static unsigned GetIndex(Addr addr) {
		return ((UInt64)addr >> SEGMENT_SHIFT) & SEGMENT_MASK;
	}

	static void* operator new(size_t size);
	static void operator delete(void* ptr);
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
	unsigned garbage_collection_period;

	void initGarbageCollector(unsigned period);
	void runGarbageCollector(Version *curr_versions, int size);

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

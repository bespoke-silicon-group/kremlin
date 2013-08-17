#ifndef _LEVELTABLE_HPP_
#define _LEVELTABLE_HPP_

#include <cassert>
#include "ktypes.h"
#include "TimeTable.hpp" // for TimeTable::TableType

//TODO: need to support dynamically growing / shrinking without MAX_LEVEL
class LevelTable {
private:
	static const unsigned MAX_LEVEL = 64;
	Version	versions[LevelTable::MAX_LEVEL];	//!< version for each level
	TimeTable* time_tables[LevelTable::MAX_LEVEL];	//!< TimeTable for each level
	bool compressed; //!< Indicates if this table has compressed TimeTables
	UInt32 code; // TODO: this should be debug-only or just go away

public:
	/*!
	 * Default, no-argument constructor. Sets all versions to 0 and all
	 * time_tables to NULL.
	 */
	LevelTable();

	~LevelTable();

	bool isCompressed() { return this->compressed; }

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
	 * @post compressed is 1.
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

#endif // _LEVELTABLE_HPP_

#ifndef _MEMORYSEGMENT_HPP_
#define _MEMORYSEGMENT_HPP_

#include "ktypes.h"

class LevelTable;

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

#endif // _MEMORYSEGMENT_HPP_

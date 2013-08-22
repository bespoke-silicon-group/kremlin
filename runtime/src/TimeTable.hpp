#ifndef _TIMETABLE_HPP_
#define _TIMETABLE_HPP_

#include <cstddef> // for size_t
#include <cstring>

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

#endif // _TIMETABLE_HPP_

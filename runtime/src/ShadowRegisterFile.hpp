#ifndef __SHADOW_REGISTER_FILE_HPP__
#define __SHADOW_REGISTER_FILE_HPP__


#include "ktypes.h"

class Table;

/*
 * Implementation of register shadowing. 
 * We use a simple 2D array for shadow register file table (Table).
 * 
 * 1) Unlike ShadowMemory, ShadowRegisterFile does not use versioning because
 * all register entries are going to be written and they should be cleaned
 * before reused.
 * This allows low overhead shadow memory operation.
 *
 * 2) Unlike ShadowMemory, ShadowRegisterFile does not require dynamic resizing.
 * The size of a RShadow Table (LTable) is determined by # of vregs and index
 * depth - they are all available when the LTable is created.
 *
 * 3) If further optimization is desirable, it is possible to use a special
 * memory allocator for LTable so that we can reduce calloc time from critical
 * path.
 * However, I doubt if it will make a big impact, as LTable creation is not a
 * common operation compared to others.
 *
 */
class ShadowRegisterFile {
public:
	ShadowRegisterFile() : times(nullptr) {}

	Time getRegisterTimeAtIndex(Reg reg, Index index);
	void setRegisterTimeAtIndex(Time time, Reg reg, Index index);
	void zeroRegistersAtIndex(Index index);

	void setTable(Table* table) { times = table; }
	Table* getTable() { return times; }

private:
	Table* times;
};


#endif

#include "Table.h"
#include "MemMapAllocator.h"
#include "RShadow.h"

/*
 * Register Shadow Memory 
 */

inline Time ShadowRegisterFile::getRegisterTimeAtIndex(Reg reg, Index index) {
	MSG(3, "RShadowGet [%d, %d] in table [%d, %d]\n",
		reg, index, times->row, times->col);
	assert(reg < times->row);	
	assert(index < times->col);
	int offset = times->getOffset(reg, index);
	Time ret = times->array[offset];
	return ret;
}

inline void ShadowRegisterFile::setRegisterTimeAtIndex(Time time, Reg reg, Index index) {
	MSG(3, "RShadowSet [%d, %d] in table [%d, %d]\n",
		reg, index, times->row, times->col);
	assert(reg < times->row);
	assert(index < times->col);
	int offset = times->getOffset(reg, index);
	MSG(3, "RShadowSet: dest = 0x%x value = %d reg = %d index = %d offset = %d\n", 
		&(times->array[offset]), time, reg, index, offset);
	assert(times != NULL);
	times->array[offset] = time;
}


inline void ShadowRegisterFile::zeroRegistersAtIndex(Index index) {
	if (index >= times->col)
		return;

	MSG(3, "zeroRegistersAtIndex col [%d] in table [%d, %d]\n",
		index, times->row, times->col);
	Reg i;
	assert(times != NULL);
	for (i=0; i<times->row; i++) {
		setRegisterTimeAtIndex(0ULL, i, index);
	}
}

#include "Table.h"
#include "MemMapAllocator.h"
#include "RShadow.h"

/*
 * Register Shadow Memory 
 */

inline Time ShadowRegisterFile::getRegisterTimeAtIndex(Reg reg, Index index) {
	MSG(3, "RShadowGet [%d, %d] in table [%d, %d]\n",
		reg, index, times->getRow(), times->getCol());
	assert(reg < times->getRow());	
	assert(index < times->getCol());
	Time ret = times->getValue(reg, index);
	return ret;
}

inline void ShadowRegisterFile::setRegisterTimeAtIndex(Time time, Reg reg, Index index) {
	MSG(3, "RShadowSet [%d, %d] in table [%d, %d]\n",
		reg, index, times->getRow(), times->getCol());
	assert(reg < times->getRow());
	assert(index < times->getCol());
	shadow_reg_file->setValue(time, reg, index);
	//MSG(3, "RShadowSet: dest = 0x%x value = %d reg = %d index = %d offset = %d\n", 
	//	&(times->array[offset]), time, reg, index, offset);
}


inline void ShadowRegisterFile::zeroRegistersAtIndex(Index index) {
	if (index >= times->getCol())
		return;

	MSG(3, "zeroRegistersAtIndex col [%d] in table [%d, %d]\n",
		index, times->getRow(), times->getCol());
	Reg i;
	assert(times != NULL);
	for (i=0; i<times->getRow(); i++) {
		setRegisterTimeAtIndex(0ULL, i, index);
	}
}

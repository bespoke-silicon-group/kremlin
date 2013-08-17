#include <cassert>
#include "debug.h"
#include "MemMapAllocator.h"
#include "TimeTable.hpp"
#include "MShadowStat.h" // for mem shadow event counters

void* TimeTable::operator new(size_t size) {
	return MemPoolAllocSmall(sizeof(TimeTable));
}

void TimeTable::operator delete(void* ptr) {
	MemPoolFreeSmall(ptr, sizeof(TimeTable));
}

unsigned TimeTable::getIndex(Addr addr) {
	const int WORD_SHIFT = 2;
	int ret = ((UInt64)addr >> WORD_SHIFT) & TimeTable::TIMETABLE_MASK;
	if (this->type == TYPE_64BIT) ret >>= 1;

	assert((this->type == TYPE_64BIT && ret < TimeTable::TIMETABLE_SIZE/2) 
			|| ret < TimeTable::TIMETABLE_SIZE);
	return ret;
}

TimeTable::TimeTable(TimeTable::TableType size_type) : type(size_type) {
	this->array = (Time*)MemPoolAlloc();
	unsigned size = TimeTable::GetNumEntries(size_type);
	memset(this->array, 0, sizeof(Time) * size);

	this->size = sizeof(Time) * TIMETABLE_SIZE / 2; // XXX: hardwired for 64?

	eventTimeTableAlloc(size_type, this->size);
	assert(this->array != NULL);
}


// XXX: this used to have an "isCompressed" arg that wasn't used
TimeTable::~TimeTable() {
	eventTimeTableFree(this->type, this->size);

	MemPoolFree(this->array);
	this->array = NULL;
}

// TODO: Replace with a function that modifies this TimeTable rather than
// creating a new one
TimeTable* TimeTable::create32BitClone() {
	assert(this->type == TimeTable::TYPE_64BIT);
	TimeTable* ret = new TimeTable(TimeTable::TYPE_32BIT);
	for (unsigned i = 0; i < TIMETABLE_SIZE/2; ++i) {
		ret->array[i*2] = this->array[i];
		ret->array[i*2 + 1] = this->array[i];
	}
	return ret;
}

void TimeTable::setTimeAtAddr(Addr addr, 
								Time time, 
								TimeTable::TableType access_type) {
	assert(addr != NULL);

	unsigned index = this->getIndex(addr);

	MSG(3, "TimeTableSet to addr 0x%llx with index %d\n", 
			&array[index], index);
	MSG(3, "\t table addr = 0x%llx, array addr = 0x%llx\n", 
			this, &array[0]);

	array[index] = time;
	if (this->type == TimeTable::TYPE_32BIT 
		&& access_type == TimeTable::TYPE_64BIT) {
		array[index+1] = time;
	}
}

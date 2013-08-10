#ifndef _TABLE_H
#define _TABLE_H

#include "ktypes.h"
#include "debug.h"
#include "MemMapAllocator.h"

#include <cstdlib> // for calloc

class Table {
private:
	int	row;
	int col;
	Time* array;

	inline int getOffset(int row, int col);

public:

	Table(int row, int col) : row(row), col(col) {
		// TRICKY: time array should be initialized with zero
		this->array = (Time*) calloc(row * col, sizeof(Time)); // TODO: use custom mem allocator
		MSG(3, "TableCreate: this = 0x%llx row = %d, col = %d\n", this, row, col);
		MSG(3, "TableCreate: this->array = 0x%llx \n", this->array);
	}

	~Table() {
		free(this->array);
	}

	inline int	getRow() { return this->row; }
	inline int	getCol() { return this->col; }

	inline Time* getElementAddr(int row, int col);
	inline Time getValue(int row, int col);
	inline void setValue(Time time, int row, int col);

	/*!
	 * Copy values of a register to another table
	 *
	 * @pre dest_table is non-NULL
	 * @pre start is less than number of columns in both this table and the
	 * destination table
	 */
	inline void copyToDest(Table* dest_table, Reg dest_reg, Reg src_reg, 
							unsigned start, unsigned size);

	static void* operator new(size_t size) {
		return (Table*)MemPoolAllocSmall(sizeof(Table));
	}

	static void operator delete(void* ptr) {
		MemPoolFreeSmall(ptr, sizeof(Table));
	}
};


int Table::getOffset(int row, int col) {
	assert(row < this->row);
	assert(col < this->col);
	return (this->col * row + col);
}

Time* Table::getElementAddr(int row, int col) {
	assert(row < this->row);
	assert(col < this->col);
	MSG(3, "TableGetElementAddr\n");
	int offset = this->getOffset(row, col);
	Time* ret = &(this->array[offset]);
	return ret;
}


Time Table::getValue(int row, int col) {
	assert(row < this->row);
	assert(col < this->col);
	MSG(3, "TableGetValue\n");
	int offset = this->getOffset(row, col);
	Time ret = this->array[offset];
	return ret;
}

void Table::setValue(Time time, int row, int col) {
	assert(row < this->row);
	assert(col < this->col);
	MSG(3, "TableSetValue\n");
	int offset = this->getOffset(row, col);
	this->array[offset] = time;
}

void Table::copyToDest(Table* dest_table, Reg dest_reg, Reg src_reg, 
						unsigned start, unsigned size) {
	Table* src_table = this;
	assert(dest_table != NULL);
	assert(start < dest_table->getCol());
	assert(start < this->getCol());

	MSG(3, "TableCopy: src_table(%d from [%d,%d]) dest_table(%d from [%d,%d]) start = %d, size = %d\n", 
		src_reg, src_table->getRow(), src_table->getCol(), dest_reg, dest_table->getRow(), dest_table->getCol(), 
		start, size);

	if (size == 0) return;

	Time* srcAddr = src_table->getElementAddr(src_reg, start);
	Time* destAddr = dest_table->getElementAddr(dest_reg, start);
	memcpy(destAddr, srcAddr, size * sizeof(Time));
}

#endif

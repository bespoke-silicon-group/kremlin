#ifndef _TABLE_H
#define _TABLE_H
#include "debug.h"
#include "ktypes.h"


class Table {
public:
	int	row;
	int col;
	Time* array;

	inline int	getRow() { return this->row; }
	inline int	getCol() { return this->col; }

	static inline Table* create(int row, int col);
	static inline void destroy(Table* table);

	inline int getOffset(int row, int col);
	inline Time* getElementAddr(int row, int col);
	inline Time getValue(int row, int col);
	inline void setValue(Time time, int row, int col);
	inline void copyToDest(Table* destTable, int destReg, int srcReg, int start, int size);
};

Table* Table::create(int row, int col) {
	Table* ret = (Table*) malloc(sizeof(Table));
	ret->row = row;
	ret->col = col;
	// should be initialized with zero
	ret->array = (Time*) calloc(row * col, sizeof(Time));
	MSG(3, "TableCreate: ret = 0x%llx row = %d, col = %d\n", ret, row, col);
	MSG(3, "TableCreate: ret->array = 0x%llx \n", ret->array);
	return ret;
}

void Table::destroy(Table* table) {
	assert(table != NULL);
	assert(table->array != NULL);

	free(table->array);
	free(table);
}

int Table::getOffset(int row, int col) {
	assert(row < this->row);
	assert(col < this->col);
	int offset = this->col * row + col;
	return offset;
}

Time* Table::getElementAddr(int row, int col) {
	MSG(3, "TableGetElementAddr\n");
	int offset = this->getOffset(row, col);
	Time* ret = &(this->array[offset]);
	return ret;
}


Time Table::getValue(int row, int col) {
	MSG(3, "TableGetValue\n");
	assert(this != NULL);
	int offset = this->getOffset(row, col);
	Time ret = this->array[offset];
	return ret;
}

void Table::setValue(Time time, int row, int col) {
	MSG(3, "TableSetValue\n");
	assert(this != NULL);
	int offset = this->getOffset(row, col);
	this->array[offset] = time;
}

/*
 * Copy values of a register to another table
 */
void Table::copyToDest(Table* destTable, int destReg, int srcReg, int start, int size) {
	Table* srcTable = this;
	MSG(3, "TableCopy: srcTable(%d from [%d,%d]) destTable(%d from [%d,%d]) start = %d, size = %d\n", 
		srcReg, srcTable->row, srcTable->col, destReg, destTable->row, destTable->col, 
		start, size);
	assert(destTable != NULL);
	assert(srcTable != NULL);
	int indexDest = destTable->getOffset(destReg, start);
	int indexSrc = srcTable->getOffset(srcReg, start);

	if (size == 0)
		return;
	assert(size >= 0);
	assert(start < destTable->col);
	assert(start < srcTable->col);
	assert(indexSrc < srcTable->row * srcTable->col);
	assert(indexDest < destTable->row * destTable->col);

	Time* srcAddr = (Time*)&(srcTable->array[indexSrc]);
	Time* destAddr = (Time*)&(destTable->array[indexDest]);
	memcpy(destAddr, srcAddr, size * sizeof(Time));
}


#endif



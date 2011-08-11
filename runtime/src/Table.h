#ifndef _TABLE_H
#define _TABLE_H
#include "defs.h"


typedef struct _Table {
	int	row;
	int col;
	Time* array;

} Table;

#if 0
Table*	TableCreate(int row, int col);
void	TableFree(Table* table);

int		TableGetRow(Table* table);
int		TableGetCol(Table* table);
Table* TableResize(int row, int col);

Time*	TableGetElementAddr(Table* table, int row, int col);
Time	TableGetValue(Table* table, int row, int col);
void 	TableSetValue(Table* table, Time time, int row, int col) {

Time	TableSetRow(Table* table, int row, int size, Time* values);
void	TableCopy(Table* destTable, int destRow, Table* srcTable, int srcRow, int start, int size);
#endif


inline Table* TableCreate(int row, int col) {
	Table* ret = (Table*) malloc(sizeof(Table));
	ret->row = row;
	ret->col = col;
	// should be initialized with zero
	ret->array = (Time*) calloc(row * col, sizeof(Time));
	MSG(1, "TableCreate: ret = 0x%llx numEntry = %d, depth = %d\n", ret, numEntry, depth);
	return ret;
}


inline void TableFree(Table* table) {
	assert(table != NULL);
	assert(table->array != NULL);

	free(table->array);
	free(table);
}

inline int		TableGetRow(Table* table) {
	return table->row;
}

inline int		TableGetCol(Table* table) {
	return table->col;
}

inline int TableGetOffset(Table* table, int row, int col) {
	int offset = table->col * row + col;
	return offset;
}

inline Time* TableGetElementAddr(Table* table, int row, int col) {
	int offset = TableGetOffset(table, row, col);
	return &(table->array[offset]);
}


inline Time TableGetValue(Table* table, int row, int col) {
	assert(table != NULL);
	int offset = TableGetOffset(table, row, col);
	Time ret = table->array[offset];
	return ret;
}

inline void TableSetValue(Table* table, Time time, int row, int col) {
	assert(table != NULL);
	int offset = TableGetOffset(table, row, col);
	table->array[offset] = time;
}

/*
 * Copy values of a register to another table
 */
inline void TableCopy(Table* destTable, int destReg, Table* srcTable, int srcReg, int start, int size) {
	assert(destTable != NULL);
	assert(srcTable != NULL);
	int indexDest = TableGetOffset(destTable, destReg, start);
	int indexSrc = TableGetOffset(srcTable, srcReg, start);
	MSG(0, "RShadowCopy: indexDest = %d,indexSrc = %d, start = %d, size = %d\n", indexDest, indexSrc, start, size);
	if (size == 0)
		return;
	assert(size >= 0);
	assert(start < destTable->indexSize);
	assert(start < srcTable->indexSize);

	Time* srcAddr = (Time*)&(srcTable->array[indexSrc]);
	Time* destAddr = (Time*)&(destTable->array[indexDest]);
	memcpy(destAddr, srcAddr, size * sizeof(Time));
}


#endif



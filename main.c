#include <assert.h>
#include "defs.h"


int cnt = 0;

int main() {
	initProfiler(10);
	testTable();
	deinitProfiler();
}

int testTable() {
	LTable* table = allocLocalTable(1000, 10);
	setLocalTable(table);
	TEntry* entry = getLTEntry(30);
	UInt64 value = getTimestamp(entry, 5, 10);
	updateTimestamp(entry, 5, 10, 1234);
	entry = getLTEntry(30);
	UInt64 value2 = getTimestamp(entry, 5, 10);
	printf("hello world %lld %lld\n", value, value2);

	entry = getGTEntry(&cnt);
	UInt64 value3 = getTimestamp(entry, 5, 10);
	updateTimestamp(entry, 5, 10, 1234);
	TEntry* entry2 = getGTEntry(&cnt);
	assert(entry == entry2);
	UInt64 value4 = getTimestamp(entry, 5, 10);
	printf("hello world %lld %lld\n", value3, value4);

#if 0
	LTable* myTable = allocLocalTable(10, 30);
	updateLocalTime(myTable, 5, 1234);
	UInt64 value = getLocalTime(myTable, 5);
	updateLocalTime(myTable, 5, 4321);
	UInt64 value2 = getLocalTime(myTable, 5);
	
	printf("hello world %lld %lld\n", value, value2);
	freeLocalTable(myTable);
	
	GTable* gTable = allocGlobalTable();
	updateGlobalTime(gTable, &cnt, 1234);
	UInt64 value3 = getGlobalTime(gTable, &cnt);
	updateGlobalTime(gTable, &cnt, 4321);
	UInt64 value4 = getGlobalTime(gTable, &cnt);
	
	printf("hello world %lld %lld\n", value3, value4);
	
	
	freeGlobalTable(gTable);
#endif
}

#include "MShadowLow.h"
#include <string.h>

#include "miniz.c" // for compress() and uncompress()

void printTArray(Time* tArray) {
	int i;
	for(i = 0; i < TIMETABLE_SIZE/2; ++i) {
		fprintf(stderr,"%d: %llu (addr: %p)\n",i,tArray[i],&tArray[i]);
	}
}

void printTimeTable(TimeTable* tTable) {
	printTArray(tTable->array);
}


// Compares each array entry in two timetables to ensure they are the same
UInt8 tArrayIsDiff(Time *array1, Time *array2) {
	UInt8 wasDiff = 0;
	int i;
	for(i = 0; i < TIMETABLE_SIZE/2; ++i) {
		if(array1[i] != array2[i]) {
			wasDiff = 1;
			fprintf(stderr,"ERROR: mismatch in timetables!\n");
			fprintf(stderr,"\ttt1[%d] = %llu, tt2[%d] = %llu\n",i,array1[i],i,array2[i]);
		}
	}

	return wasDiff;
}

UInt8* compressData(UInt8* src, uLong sizeSrc, uLong* sizeDest) {
	assert(src != NULL);
	assert(sizeSrc > 0);
	assert(sizeDest != NULL);

	//uLong compLen = compressBound(sizeSrc);
	*sizeDest = sizeSrc;
	UInt8* dest = malloc(sizeSrc); // TODO: move away from malloc/free
	//fprintf(stderr, "[enc] compress src=0x%llx, dest=0x%llx, srcSize = %d\n", src, dest, sizeSrc);
	int compStatus = compress(dest, sizeDest, src, sizeSrc);
	//fprintf(stderr, "[enc] result = %d sizeDest = %d\n", compStatus, *sizeDest);
	assert(compStatus == Z_OK);
	return realloc(dest, *sizeDest);
}

void decompressData(UInt8* dest, UInt8* src, uLong sizeSrc, uLong* sizeDest) {
	assert(src != NULL);
	assert(dest != NULL);
	assert(sizeSrc > 0);
	assert(sizeDest != NULL);
//	*sizeDest = sizeSrc;

	//fprintf(stderr, "[dec] decompress src=0x%llx, dest=0x%llx, srcSize = %d\n", src, dest, sizeSrc);
	int compStatus = uncompress(dest, sizeDest, src, sizeSrc);
	//fprintf(stderr, "[dec] result = %d sizeDest = %d\n", compStatus, *sizeDest);
	assert(compStatus == Z_OK);
	free(src);
}

#if 0
UInt64 compressLTable(LTable* lTable) {
	if(lTable->isCompressed == 1) return 0;

	int i;
	uLong outLen = 0;
	uLong finalLen = 0;
	//for(i = 0; i < MAX_LEVEL; ++i) {
	for(i = 0; i < 1; ++i) {
		TimeTable* tt2 = lTable->tArray[i];
		if(tt2 == NULL) { 
			break; 
		} // NULL indicates we aren't using this level

		uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2; 
		UInt8* buf = compressData((UInt8*)(tt2->array), srcLen, &outLen); 
		//UInt8* buf = compressData(buffer, srcLen, &outLen); 
		
		decompressData((UInt8*)tt2->array, buf, outLen, &finalLen);
		//decompressData(buffer, buf, outLen, &finalLen);
		assert(finalLen == srcLen);

	}

	return 0;
}

UInt64 decompressLTable(LTable* lTable) {
	return 0;
}
#endif

#if 1
// we'll assume you already GC'ed lTable... otherwise you are going to be
// doing useless work (i.e. compressing data that is out of date)
// Returns: number of bytes saved by compression
UInt64 compressLTable(LTable* lTable) {
	if(lTable->isCompressed == 1) return 0;

	TimeTable* tt1 = lTable->tArray[0];
	if(tt1 == NULL) return 0;
	//fprintf(stderr,"compressing LTable (%p)\n",lTable);

	UInt64 compressionSavings = 0;

	uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2; // XXX assumes 8 bytes
	uLong compLen = 0;
	void* compressedData = compressData((UInt8*)tt1->array, srcLen, &compLen);

	Time* level0Array = tt1->array;
	tt1->array = compressedData;
	compressionSavings += (srcLen - compLen);
	lTable->compressedLen[0] = compLen;

	//fprintf(stderr,"compressed level 0 data to %u bytes\n",compLen);
	Time* diffs = MemPoolAlloc();

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
			
		// step 1: create/fill in time difference table
		TimeTable* tt2 = lTable->tArray[i];

		if(tt2 == NULL) { break; } // NULL indicates we aren't using this level
		//lTable->tArrayBackup[i] = tt2->array;

		//fprintf(stderr,"compressing level %d\n",i);
		//printTimeTable(tt2);

		// for now, we'll always diff based on level 0
		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			diffs[j] = level0Array[j] - tt2->array[j];
			//fprintf(stderr,"diffs[%d] = %llu\n",j,diffs[j]);
		}

		// step 2: compress diffs
		compLen = 0;
		compressedData = compressData((UInt8*)diffs, srcLen, &compLen);

		//fprintf(stderr,"compressed data to %u bytes\n",compLen);
		compressionSavings += (srcLen - compLen);
		lTable->compressedLen[i] = compLen;

		//fprintf(stderr,"compressed level %d data to %u bytes\n",i,compLen);

		// step 3: profit
		MemPoolFree(tt2->array); // XXX: comment this out if using tArrayBackup
		tt2->array = compressedData;
	}

	MemPoolFree(level0Array);
	MemPoolFree(diffs);

	lTable->isCompressed = 1;
	//fprintf(stderr,"finished compressing LTable\n");

	//fprintf(stderr,"saved %llu bytes from compression\n",compressionSavings);
	return compressionSavings;
}


UInt64 decompressLTable(LTable* lTable) {
	if(lTable->isCompressed == 0) return 0;

	UInt64 decompressionCost = 0;

	//fprintf(stderr,"decompressing LTable (%p)\n",lTable);
	uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2;
	uLong uncompLen = srcLen;

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = lTable->tArray[0];
	//fprintf(stderr,"decompressing level 0\n");

	Time* decompedArray = MemPoolAlloc();
	//int compStatus = uncompress((UInt8*)decompedArray, &uncompLen, (UInt8*)tt1->array, lTable->compressedLen[0]);
	decompressData((UInt8*)decompedArray, (UInt8*)tt1->array, lTable->compressedLen[0], &uncompLen);

	//free(tt1->array); // TODO: move away from free/malloc
	tt1->array = decompedArray;
	//printTimeTable(tt1);

	decompressionCost += (srcLen - lTable->compressedLen[0]);
	lTable->compressedLen[0] = 0;

	Time *diffs = MemPoolAlloc();

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
		TimeTable* tt2 = lTable->tArray[i];
		if(tt2 == NULL) break;
		//fprintf(stderr,"decompressing level %d\n",i);

		// step 1: decompress time different table
		uncompLen = srcLen;
		//int compStatus = uncompress((UInt8*)diffs, &uncompLen, (UInt8*)tt2->array, lTable->compressedLen[i]);
		decompressData((UInt8*)diffs, (UInt8*)tt2->array, lTable->compressedLen[i], &uncompLen);
		//assert(compStatus == Z_OK);
		assert(srcLen == uncompLen);

		decompressionCost += (srcLen - lTable->compressedLen[i]);
		lTable->compressedLen[i] = 0;

		// step 2: add diffs to base TimeTable
		//free(tt2->array); // TODO: move away from malloc/free
		tt2->array = MemPoolAlloc();

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			//fprintf(stderr,"diffs[%d] = %llu\n",j,diffs[j]);
			tt2->array[j] = tt1->array[j] - diffs[j];
		}

		//UInt8 wasDiff = tArrayIsDiff(tt2->array,lTable->tArrayBackup[i]);
		//assert(wasDiff == 0);
		//MemPoolFree(lTable->tArrayBackup[i]);
	}

	// clean up diffs
	MemPoolFree(diffs);

	lTable->isCompressed = 0;
	//fprintf(stderr,"finished decompressing LTable (%p)\n",lTable);

	return decompressionCost;
}
#endif

static UInt32 uncompressedTables = 0;
static UInt32 compressedTables = 0;

inline UInt64 calculateLTableOverheadCompressed(LTable* lTable) {
	UInt64 overhead = 0;

	int i;
	for(i = 0; i < MAX_LEVEL; ++i) {
		if(lTable->tArray[i] == NULL) break;
		else {
			overhead += lTable->compressedLen[i];
		}
	}

	return overhead;
}

inline UInt64 calculateLTableOverheadUncompressed(LTable* lTable) {
	UInt64 overhead = 0;

	int i;
	for(i = 0; i < MAX_LEVEL; ++i) {
		if(lTable->tArray[i] == NULL) break;
		else {
			overhead += sizeof(Time)*TIMETABLE_SIZE/2;
		}
	}

	return overhead;
}

inline UInt64 calculateLTableOverhead(LTable* lTable) {
	UInt64 overhead;

	if(lTable->isCompressed == 1)  {
		compressedTables++;
		overhead = calculateLTableOverheadCompressed(lTable);
	}
	else {
		uncompressedTables++;
		overhead = calculateLTableOverheadUncompressed(lTable);
	}

	return overhead;
}

UInt64 calculateTimeTableOverhead() {
	uncompressedTables = compressedTables = 0;
	UInt64 overhead = 0;
	int i;
	for(i = 0; i < sTable.writePtr; ++i) {
		SegTable* currSeg = sTable.entry[i].segTable;

		int j;
		for(j = 0; j < SEGTABLE_SIZE; ++j) {
			LTable* currLT = currSeg->entry[j];
			if(currLT != NULL) {
				overhead += calculateLTableOverhead(currLT);
			}
		}
	}
	fprintf(stderr,"%u uncompressed, %u compressed\n",uncompressedTables,compressedTables);
	//fprintf(stderr,"finished compression (saved %u bytes)\n",newCompressionSavings);

	return overhead;
}

UInt64 compressShadowMemory(Version* vArray) {
	//fprintf(stderr,"beginning compression\n");
	UInt64 newCompressionSavings = 0;
	int i;
	for(i = 0; i < sTable.writePtr; ++i) {
		SegTable* currSeg = sTable.entry[i].segTable;

		int j;
		for(j = 0; j < SEGTABLE_SIZE; ++j) {
			LTable* currLT = currSeg->entry[j];
			if(currLT != NULL) {
				newCompressionSavings += compressLTable(currLT);
			}
		}
	}
	//fprintf(stderr,"finished compression (saved %u bytes)\n",newCompressionSavings);

	return newCompressionSavings;
}

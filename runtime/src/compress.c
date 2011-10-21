#include "MShadowLow.h"
#include <string.h>

#include "miniz.c" // for compress() and uncompress()

static UInt64 totalCompressionSavings = 0;

void printTimeTable(TimeTable* tTable) {
	int i;
	for(i = 0; i < TIMETABLE_SIZE/2; ++i) {
		fprintf(stderr,"%d: %llu\n",i,tTable->array[i]);
	}
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

// we'll assume you already GC'ed lTable... otherwise you are going to be
// doing useless work (i.e. compressing data that is out of date)
// Returns: number of bytes saved by compression
void compressLTable(LTable* lTable, Version* vArray) {
	if(lTable->isCompressed == 1) return;

	TimeTable* tt1 = lTable->tArray[0];
	if(tt1 == NULL) return;
	//fprintf(stderr,"compressing LTable (%p)\n",lTable);

	uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2; // XXX assumes 8 bytes
	uLong compLen = compressBound(srcLen);
	void* compressedData = malloc(compLen);

	int compStatus = compress((UInt8*)compressedData,&compLen,(UInt8*)tt1->array,srcLen);
	assert(compStatus == Z_OK);

	free(tt1->array);
	tt1->array = realloc(compressedData,compLen);
	totalCompressionSavings += (srcLen - compLen);
	lTable->compressedLen[0] = compLen;

	//fprintf(stderr,"compressed level 0 data to %u bytes\n",compLen);

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
			
		// step 1: create/fill in time difference table
		TimeTable* tt2 = lTable->tArray[i];

		if(tt2 == NULL) { break; } // NULL indicates we aren't using this level
		//lTable->tArrayBackup[i] = tt2->array;

		//fprintf(stderr,"compressing level %d\n",i);

		// XXX this is assuming that always using 8 byte entries
		Time* diffs = malloc(srcLen);

		// for now, we'll always diff based on level 0
		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			diffs[j] = tt1->array[j] - tt2->array[j];
		}

		// step 2: compress diffs
		compLen = compressBound(srcLen);
		//lTable->tArray[i]->array = malloc(compLen);
		compressedData = malloc(compLen);
		compStatus = compress((UInt8*)compressedData,&compLen,(UInt8*)diffs,srcLen);

		assert(compStatus == Z_OK);
		if(compStatus != Z_OK) {
			fprintf(stderr,"ERROR: compression failed (status: %d)!\n",compStatus);
		}

		//fprintf(stderr,"compressed data to %u bytes\n",compLen);
		
		totalCompressionSavings += (srcLen - compLen);
		lTable->compressedLen[i] = compLen;

		//fprintf(stderr,"compressed timetable to %u bytes\n",compLen);

		// step 3: profit
		free(diffs);
		free(tt2->array);
		tt2->array = realloc(compressedData,compLen);
		//tt2->array = compressedData;
		//void* newCompressedData = malloc(compLen);
		//memcpy(newCompressedData,compressedData,compLen);
		//tt2->array = newCompressedData;
		//free(compressedData);
	}

	lTable->isCompressed = 1;
	//fprintf(stderr,"finished compressing LTable\n");
}


void decompressLTable(LTable* lTable) {
	if(lTable->isCompressed == 0) return;
	//fprintf(stderr,"decompressing LTable (%p)\n",lTable);
	uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2;
	uLong uncompLen = srcLen;

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = lTable->tArray[0];

	Time* decompedArray = malloc(srcLen);
	int compStatus = uncompress((UInt8*)decompedArray, &uncompLen, (UInt8*)tt1->array, lTable->compressedLen[0]);

	free(tt1->array);
	tt1->array = decompedArray;

	totalCompressionSavings -= (srcLen - lTable->compressedLen[0]);

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
		TimeTable* tt2 = lTable->tArray[i];
		if(tt2 == NULL) break;
		//fprintf(stderr,"decompressing level %d\n",i);

		// step 1: decompress time different table
		//uLong compLen = compressBound(srcLen);
		uncompLen = srcLen;
		Time *diffs = malloc(srcLen);
		compStatus = uncompress((UInt8*)diffs, &uncompLen, (UInt8*)tt2->array, lTable->compressedLen[i]);

		assert(compStatus == Z_OK);
		assert(srcLen == uncompLen);

		if(compStatus != Z_OK) {
			fprintf(stderr,"ERROR: decompression failed (status: %d)!\n",compStatus);
		}
		if(srcLen != uncompLen) {
			fprintf(stderr,"\tWARNING: uncompressed to size %u (expected %u)",uncompLen,srcLen);
		}

		totalCompressionSavings -= (srcLen - lTable->compressedLen[i]);

		// step 2: add diffs to base TimeTable
		free(tt2->array);
		tt2->array = malloc(srcLen);

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			tt2->array[j] = tt1->array[j] - diffs[j];
		}

		//UInt8 wasDiff = tArrayIsDiff(tt2->array,lTable->tArrayBackup[i]);
		//if(wasDiff == 1) fprintf(stderr,"ERROR: mismatch!\n");

		// step 3: clean up diffs
		free(diffs);
		diffs = NULL;
	}

	lTable->isCompressed = 0;
	//fprintf(stderr,"finished decompressing LTable (%p)\n",lTable);
}

void compressShadowMemory(Version* vArray) {
	fprintf(stderr,"beginning compression\n");
	int i;
	for(i = 0; i < sTable.writePtr; ++i) {
		SegTable* currSeg = sTable.entry[i].segTable;

		int j;
		for(j = 0; j < SEGTABLE_SIZE; ++j) {
			LTable* currLT = currSeg->entry[j];
			if(currLT != NULL) {
				gcLevel(currLT,vArray);
				compressLTable(currLT, vArray);
			}
		}
	}
	fprintf(stderr,"finished compression (total savings from compression: %u bytes)\n",totalCompressionSavings);
}

#include "MShadowLow.h"

#include "miniz.c" // for compress() and uncompress()

void printTimeTable(TimeTable* tTable) {
	int i;
	for(i = 0; i < TIMETABLE_SIZE/2; ++i) {
		fprintf(stderr,"%d: %llu\n",i,tTable->array[i]);
	}
}

// Compares each array entry in two timetables to ensure they are the same
UInt8 checkLossless(TimeTable *tt1, TimeTable *tt2) {
	UInt8 wasDiff = 0;
	int i;
	for(i = 0; i < TIMETABLE_SIZE/2; ++i) {
		if(tt1->array[i] != tt2->array[i]) {
			wasDiff = 1;
			fprintf(stderr,"ERROR: mismatch in timetables!\n");
			fprintf(stderr,"\ttt1[%d] = %llu, tt2[%d] = %llu\n",i,tt1->array[i],i,tt2->array[i]);
		}
	}

	return wasDiff;
}

// we'll assume you already GC'ed lTable... otherwise you are going to be
// doing useless work (i.e. compressing data that is out of date)
// Returns: number of bytes used by timetables
UInt64 compressLTable(LTable* lTable, Version* vArray) {
	if(lTable->isCompressed == 1) return 0;
	//fprintf(stderr,"compressing LTable (%p)\n",lTable);

	UInt64 compressedSize = 0;
	uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2; // XXX assumes 8 bytes

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = lTable->tArray[0];

	compressedSize += srcLen;

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
			
		// step 1: create/fill in time difference table
		TimeTable* tt2 = lTable->tArray[i];

		if(tt2 == NULL) { break; } // NULL indicates we aren't using this level

		//fprintf(stderr,"compressing level %d\n",i);

		// XXX this is assuming that always using 8 byte entries
		Time* diffs = malloc(srcLen);

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			diffs[j] = tt1->array[j] - tt2->array[j];
		}

		// step 2: compress diffs
		uLong compLen = compressBound(srcLen);
		uLong compLenEstimate = compLen;
		//lTable->tArray[i]->array = malloc(compLen);
		void* compressedData = malloc(compLen);
		int compStatus = compress((UInt8*)compressedData,&compLen,(UInt8*)diffs,srcLen);

		assert(compStatus == Z_OK);
		if(compStatus != Z_OK) {
			fprintf(stderr,"ERROR: compression failed (status: %d)!\n",compStatus);
		}

		//fprintf(stderr,"compressed data to %u bytes (upper bound: %u bytes)\n",compLen,compLenEstimate);
		
		compressedSize += compLen;
		lTable->compressedLen[i] = compLen;

		//fprintf(stderr,"compressed timetable to %u bytes\n",compLen);

		// step 3: profit
		free(diffs);
		free(tt2->array);
		//tt2->array = realloc(compressedData,compLen);
		tt2->array = compressedData;
	}

	lTable->isCompressed = 1;
	//fprintf(stderr,"finished compressing LTable\n");

	return compressedSize;
}


void decompressLTable(LTable* lTable) {
	if(lTable->isCompressed == 0) return;
	//fprintf(stderr,"decompressing LTable (%p)\n",lTable);

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = lTable->tArray[0];

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
		TimeTable* tt2 = lTable->tArray[i];
		if(tt2 == NULL) break;
		//fprintf(stderr,"decompressing level %d\n",i);

		// step 1: decompress time different table
		uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2;
		//uLong compLen = compressBound(srcLen);
		uLong uncompLen = srcLen;
		Time *diffs = malloc(srcLen);
		int compStatus = uncompress((UInt8*)diffs, &uncompLen, (UInt8*)tt2->array, lTable->compressedLen[i]);

		assert(compStatus == Z_OK);
		assert(srcLen == uncompLen);

		if(compStatus != Z_OK) {
			fprintf(stderr,"ERROR: decompression failed (status: %d)!\n",compStatus);
		}
		if(srcLen != uncompLen) {
			fprintf(stderr,"\tWARNING: uncompressed to size %u (expected %u)",uncompLen,srcLen);
		}

		// step 2: add diffs to base TimeTable
		free(tt2->array);
		tt2->array = malloc(srcLen);

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			tt2->array[j] = tt1->array[j] - diffs[j];
		}

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
	fprintf(stderr,"finished compression\n");
}

#include "MShadowLow.h"
#include <string.h> // for memcpy

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

void compressLTable(LTable* lTable, Version* vArray) {
	if(lTable->isCompressed == 1) return;
	fprintf(stderr,"compressing LTable (%p)\n",lTable);

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = (TimeTable*)lTable->tArray[0];

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
		// step 1: create/fill in time difference table
		TimeTable* tt2 = (TimeTable*)lTable->tArray[i];
		lTable->tArrayBackup[i] = tt2;

		if(tt2 == NULL) { break; }
		fprintf(stderr,"compressing level %d\n",i);

		// XXX this is assuming that always using 8 byte entries
		Time* diffs = malloc(sizeof(Time)*TIMETABLE_SIZE/2);

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			Time diff = tt1->array[j] - tt2->array[j];
			diffs[j] = diff;
		}

		// step 2: compress table with LZW
		uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2;
		uLong compLen = compressBound(srcLen);
		lTable->tArray[i] = malloc(compLen);
		int compStatus = compress(lTable->tArray[i],&compLen,(UInt8*)diffs,srcLen);

		assert(compStatus == Z_OK);
		if(compStatus != Z_OK) {
			fprintf(stderr,"ERROR: compression failed!\n");
		}

		fprintf(stderr,"compressed timetable to %u bytes\n",compLen);

		// step 3: profit
		free(diffs);
		//lTable->tArray[i] = realloc(lTable->tArray[i],compLen);
		//TimeTableFree(tt2);
	}

	//fprintf(stderr,"num entries in ctt: %hu\n",nextDictId);

	lTable->isCompressed = 1;
	fprintf(stderr,"finished compressing LTable\n");
}


void decompressLTable(LTable* lTable) {
	if(lTable->isCompressed == 0) return;
	fprintf(stderr,"decompressing LTable (%p)\n",lTable);

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = (TimeTable*)lTable->tArray[0];

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
		fprintf(stderr,"decompressing level %d\n",i);

		// step 1: decompress time different table
		if(lTable->tArray[i] == NULL) break;

		uLong srcLen = sizeof(Time)*TIMETABLE_SIZE/2;
		uLong compLen = compressBound(srcLen);
		uLong uncompLen = srcLen;
		Time *diffs = malloc(srcLen);

		int compStatus = uncompress((UInt8*)diffs, &uncompLen, lTable->tArray[i], compLen);

		assert(compStatus == Z_OK);
		assert(srcLen == uncompLen);

		if(compStatus != Z_OK) {
			fprintf(stderr,"ERROR: decompression failed!\n");
		}
		if(srcLen != uncompLen) {
			fprintf(stderr,"\tWARNING: uncompressed to size %u (expected %u)",uncompLen,srcLen);
		}

		// step 2: add diffs to base TimeTable
		// XXX hardwired in 64 bit entries and lTable version management
		TimeTable* tt2 = TimeTableAlloc(TYPE_64BIT,0);

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			tt2->array[j] = tt1->array[j] - diffs[j];
		}

		// step 3: set tArray to decompressed TimeTable
		lTable->tArray[i] = tt2;

		// XXX see if compress/decompress was truely lossless ;)
		UInt8 wasDiff = checkLossless(tt2,lTable->tArrayBackup[i]);

		// step 4: clean up diffs and compressed time table
		free(diffs);
		diffs = NULL;
	}

	lTable->isCompressed = 0;
	fprintf(stderr,"finished decompressing LTable (%p)\n",lTable);
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

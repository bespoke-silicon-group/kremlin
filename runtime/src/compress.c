#include "MShadowLow.h"
#include <string.h> // for memcpy

#define COMPRESSOR_CHUNK_SIZE 512


// TODO: store size in diffs array for bettery memory alloc?
typedef struct _CompressedTimeTable {
	UInt32 size; // num of entries in UInt16 array
	UInt16* diffs; // compressed array of diffs
} CompressedTimeTable;

CompressedTimeTable* compressTimeTableDiff(UInt16* diffArray, DictEntry** compressionDict, DictEntry** decompressionDict, UInt16* nextDictId) {
	UInt16 *currSymbol = malloc(sizeof(UInt16)*COMPRESSOR_CHUNK_SIZE);
	unsigned int symbolArrayCapacity = COMPRESSOR_CHUNK_SIZE;
	UInt32 currSymbolLength = 0;
	UInt32 computedCompressedSize = 0;

	CompressedTimeTable *compressedTT = malloc(sizeof(CompressedTimeTable));

	// To start, the compressed array will be same size as input array.
	// Later, we'll realloc it down to a smaller size based on how much we
	// actually used.
	UInt16* compressedArray = malloc((TIMETABLE_SIZE/2)*sizeof(Time));
	unsigned int compressedIndex = 0;

	DictEntry *dEntry, *newEntry, *singleEntry, *reversedEntry;

	int i;
	for(i = 0; i < (TIMETABLE_SIZE/2)*(sizeof(Time)/sizeof(UInt16)); ++i) {
		//fprintf(stderr,"index = %d\n",i);
		fprintf(stderr,"value %d: %hu\n",i,diffArray[i]);

		UInt8 newChar = 0; // 1 => first time this short has been seen

		// if diffArray[i] doesn't exist in dictionary, add it
		HASH_FIND( hh, *compressionDict, &diffArray[i], sizeof(UInt16), dEntry );
		if(dEntry == NULL) {
			fprintf(stderr,"\thaven't seen value (%hu) before. Adding to dict with id = %hu\n",diffArray[i],*nextDictId);

			dEntry = malloc(sizeof(DictEntry));
			dEntry->name = malloc(sizeof(UInt16));
			dEntry->name[0] = diffArray[i];
			dEntry->id = *nextDictId;
			reversedEntry = malloc(sizeof(DictEntry));
			reversedEntry->name = malloc(sizeof(UInt16));
			reversedEntry->name[0] = diffArray[i];
			reversedEntry->id = *nextDictId;
			(*nextDictId)++;
			dEntry->size = 1;
			reversedEntry->size = 1;
			newChar = 1;

			HASH_ADD_KEYPTR( hh, *compressionDict, &diffArray[i], sizeof(UInt16), dEntry );
			HASH_ADD( hh, *decompressionDict, id, sizeof(UInt16), reversedEntry );
		}

		// add current short to current symbol
		currSymbol[currSymbolLength] = diffArray[i];
		currSymbolLength++;

		// special check for first time through loop
		//if(currSymbolLength == 1) { 
		/*
		if(i == 0 && newChar == 1) { 
			dEntry = singleEntry;
			continue;
		}
		*/

		fprintf(stderr,"checking symbol (len = %u): ",currSymbolLength);
		int j;
		for(j = 0; j < currSymbolLength; ++j) {
			fprintf(stderr,"%hu ",currSymbol[j]);
		}
		fprintf(stderr,"\n");

		// check if current symbol is in the dictionary
		HASH_FIND( hh, *compressionDict, currSymbol, currSymbolLength*sizeof(UInt16), dEntry);

		// if not, we need to add it and output code for previous symbol
		if(dEntry != NULL) {
			//fprintf(stderr,"\tFound existing entry in dict for current //symbol (id = %hu)\n",dEntry->id);
			//dEntry = newEntry;
		}
		else {
			fprintf(stderr,"\tSymbol not in dict: Adding with id = %hu (len = %hu)\n",*nextDictId,currSymbolLength);
			//int j;
			fprintf(stderr,"\t\tsymbol: ");
			for(j = 0; j < currSymbolLength; ++j) {
				fprintf(stderr,"%hu ",currSymbol[j]);
			}
			fprintf(stderr,"\n");


			// add new symbol to dictionary
			dEntry = malloc(sizeof(DictEntry));
			dEntry->name = malloc(sizeof(UInt16)*currSymbolLength);
			memcpy(dEntry->name,currSymbol,currSymbolLength*sizeof(UInt16));
			dEntry->id = *nextDictId;
			reversedEntry = malloc(sizeof(DictEntry));
			reversedEntry->name = malloc(sizeof(UInt16)*currSymbolLength);
			memcpy(reversedEntry->name,currSymbol,currSymbolLength*sizeof(UInt16));
			reversedEntry->id = *nextDictId;
			(*nextDictId)++;
			dEntry->size = currSymbolLength;
			reversedEntry->size = currSymbolLength;

			HASH_ADD_KEYPTR( hh, *compressionDict, currSymbol, currSymbolLength*sizeof(UInt16), dEntry );
			HASH_ADD( hh, *decompressionDict, id, sizeof(UInt16), reversedEntry );

			// store code for previous symbol in compressed array
			/*
			fprintf(stderr,"storing code for symbol (len = %u): ",currSymbolLength-1);
			int j;
			for(j = 0; j < currSymbolLength-1; ++j) {
				fprintf(stderr,"%hu ",currSymbol[j]);
			}
			fprintf(stderr,"\n");
			*/
			HASH_FIND( hh, *compressionDict, currSymbol, (currSymbolLength-1)*sizeof(UInt16), dEntry);
			compressedArray[compressedIndex] = dEntry->id;
			fprintf(stderr,"compressedArray[%u] = %hu (size = %hu)\n",compressedIndex,dEntry->id,dEntry->size);
			computedCompressedSize += dEntry->size;
			compressedIndex++;

			// reset symbol to be short we just tried appending
			bzero(currSymbol,currSymbolLength*sizeof(UInt16));
			currSymbol[0] = diffArray[i];
			currSymbolLength = 1;
		}

		// check if we need to increase size of currSymbol
		if(currSymbolLength == symbolArrayCapacity) {
			fprintf(stderr,"NOTE: increasing symbol capacity\n");
			symbolArrayCapacity += COMPRESSOR_CHUNK_SIZE;
			currSymbol = realloc(currSymbol,sizeof(UInt16)*symbolArrayCapacity);
		}

		//if(newChar == 1) dEntry = singleEntry;
	}

	// don't forget to add the last entry!
	compressedArray[compressedIndex] = dEntry->id;
	fprintf(stderr,"compressedArray[%u] = %hu (size = %hu)\n",compressedIndex,dEntry->id,dEntry->size);
	computedCompressedSize += dEntry->size;
	compressedIndex++;

	//fprintf(stderr,"computed compressed length = %u\n",computedCompressedSize);
	//fprintf(stderr,"compressedIndex = %u\n",compressedIndex);

	compressedArray = realloc(compressedArray,sizeof(UInt16)*compressedIndex);

	compressedTT->diffs = compressedArray;
	compressedTT->size = compressedIndex;

	free(currSymbol);

	return compressedTT;
}

void CompressedTimeTableFree(CompressedTimeTable* ctt) {
	free(ctt->diffs);
	free(ctt);
	ctt = NULL;
}

void decompressionDictFree(DictEntry* compDict) {
	DictEntry *currEntry, *tmp;

	HASH_ITER(hh, compDict, currEntry, tmp) {
		HASH_DEL(compDict,currEntry);
		free(currEntry->name);
		free(currEntry);
	}
}

void compressLTable(LTable* lTable, Version* vArray) {
	if(lTable->isCompressed == 1) return;
	fprintf(stderr,"compressing LTable (%p)\n",lTable);

	UInt16 nextDictId = 0;
	DictEntry* compressionDict = NULL;

	UInt16 nextDictId2 = 0;
	DictEntry* compressionDict2 = NULL;

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = (TimeTable*)lTable->tArray[0];

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
		fprintf(stderr,"compressing level %d\n",i);
		if(i == 2 && lTable == 0x6375d0) fprintf(stderr,"MEOWWWW!\n"); // XXX
		// step 1: create/fill in time difference table
		TimeTable* tt2 = (TimeTable*)lTable->tArray[i];
		lTable->tArrayBackup[i] = tt2;

		if(tt2 == NULL) break;

		// XXX this is assuming that always using 8 byte entries
		Time* diffs = malloc(sizeof(Time)*TIMETABLE_SIZE/2);

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			Time diff = tt1->array[j] - tt2->array[j];
			diffs[j] = diff;
		}

		/*
		int k;
		for(k = 0; k < TIMETABLE_SIZE/2; ++k) {
			fprintf(stderr,"diffs[%d] = %llu\n",k,diffs[k]);
		}
		*/


		//printTimeTable(tt2)

		// step 2: compress table with LZW
		//lTable->tArray[i] = compressTimeTableDiff((UInt16*)diffs, &compressionDict, &(lTable->decompressionDict), &nextDictId);
		CompressedTimeTable *ctt = compressTimeTableDiff((UInt16*)diffs, &compressionDict, &lTable->decompressionDict, &nextDictId);
		lTable->tArray[i] = ctt;

		//CompressedTimeTable *ctt2 = compressTimeTableDiff((UInt16*)diffs, &compressionDict2, &lTable->decompressionDict2, &nextDictId2);
		//lTable->tArray2[i] = ctt2;
		lTable->tArray2[i] = ctt;


		fprintf(stderr,"compressed timetable to %u UInt16 entries\n",ctt->size);

		// step 3: profit
		free(diffs);
		//TimeTableFree(tt2);
	}

	//fprintf(stderr,"num entries in ctt: %hu\n",nextDictId);
	//HASH_CLEAR(hh,compressionDict);
	decompressionDictFree(compressionDict);
	decompressionDictFree(compressionDict2);

	lTable->isCompressed = 1;
	fprintf(stderr,"finished compressing LTable\n");
}

Time* decompressTimeTableDiff(CompressedTimeTable* ctt, DictEntry* decompressionDict) {

	DictEntry *dEntry;
	UInt16* decompressedDiffs = malloc(sizeof(Time)*TIMETABLE_SIZE/2);

	UInt32 currDiffsEntry = 0;
	UInt32 computedSize = 0;

	//fprintf(stderr,"decompressing ctt with size = %u\n",ctt->size);

	int i;
	for(i = 0; i < ctt->size; ++i) {
		HASH_FIND( hh, decompressionDict, &ctt->diffs[i], sizeof(UInt16), dEntry );
		//fprintf(stderr,"ctt[%d] = %hu (size = %u)\n",i,ctt->diffs[i],dEntry->size);
		computedSize += dEntry->size;

		int j;
		for(j = 0; j < dEntry->size; ++j) {
			//fprintf(stderr,"j = %d, currDiffsEntry = %u\n",j,currDiffsEntry);
			decompressedDiffs[currDiffsEntry] = dEntry->name[j];
			//fprintf(stderr,"%u: %hu\n",currDiffsEntry,decompressedDiffs[currDiffsEntry]);
			currDiffsEntry++;
		}
	}

	// sanity checking to make sure we decompressed to the correct size
	if(currDiffsEntry != (sizeof(Time)/sizeof(UInt16))*(TIMETABLE_SIZE/2)) {
		UInt32 blah = (sizeof(Time)/sizeof(UInt16))*(TIMETABLE_SIZE/2);
		fprintf(stderr,"ERROR: decompression to incorrect size (size = %u, correct size = %u)!\n",currDiffsEntry,blah);
		fprintf(stderr,"computed size = %u\n",computedSize);
		return NULL;
	}

	return (Time*)decompressedDiffs;
}

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

void decompressLTable(LTable* lTable) {
	if(lTable->isCompressed == 0) return;
	fprintf(stderr,"decompressing LTable (%p)\n",lTable);

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = (TimeTable*)lTable->tArray[0];

	int i;
	for(i = 1; i < MAX_LEVEL; ++i) {
		fprintf(stderr,"decompressing level %d\n",i);
		if(i == 2 && lTable == 0x6375d0) fprintf(stderr,"MEOWWWW!\n");

		// step 1: decompress time different table
		CompressedTimeTable *ctt = (CompressedTimeTable*)lTable->tArray[i];
		CompressedTimeTable *ctt2 = (CompressedTimeTable*)lTable->tArray2[i];
		if(ctt == NULL) break;

		Time *diffs = decompressTimeTableDiff(ctt,lTable->decompressionDict);
		//Time *diffs2 = decompressTimeTableDiff(ctt2,lTable->decompressionDict2);
		Time *diffs2 = diffs;

		/*
		int k;
		for(k = 0; k < TIMETABLE_SIZE/2; ++k) {
			fprintf(stderr,"diffs[%d] = %llu\n",k,diffs[k]);
		}
		*/

		// step 2: add diffs to base TimeTable
		// XXX hardwired in 64 bit entries and lTable version management
		TimeTable* tt2 = TimeTableAlloc(TYPE_64BIT,0);
		TimeTable* tt3 = TimeTableAlloc(TYPE_64BIT,0);

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			tt2->array[j] = tt1->array[j] - diffs[j];
			tt3->array[j] = tt1->array[j] - diffs2[j];
		}

		// step 3: set tArray to decompressed TimeTable
		lTable->tArray[i] = tt2;

		// XXX see if compress/decompress was truely lossless ;)
		UInt8 wasDiff = checkLossless(tt2,lTable->tArrayBackup[i]);
		//UInt8 wasDiff = checkLossless(tt2,tt3);

		//printTimeTable((TimeTable*)lTable->tArray[i]);

		// step 4: clean up diffs and compressed time table
		free(diffs);
		diffs = NULL;

		free(ctt->diffs);
		free(ctt);
		ctt = NULL;
	}

	decompressionDictFree(lTable->decompressionDict);
	lTable->decompressionDict = NULL;

	//fprintf(stderr,"Decompressing lTable.\n");
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

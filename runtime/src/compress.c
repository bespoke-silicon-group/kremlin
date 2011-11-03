#include "defs.h"
#include "MShadowLow.h"
#include <string.h>
#include "minilzo.h"


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

UInt64 _compSrcSize;
UInt64 _compDestSize;

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);


static UInt8* compressData(UInt8* src, lzo_uint sizeSrc, lzo_uintp sizeDest) {
	assert(src != NULL);
	assert(sizeSrc > 0);
	assert(sizeDest != NULL);

	//XXX need a specialized memory allocator, for now, just check the 
	UInt8* dest = MemPoolAlloc();
	int result = lzo1x_1_compress(src, sizeSrc, dest, sizeDest, wrkmem);
	assert(result == LZO_E_OK);
	//memcpy(dest, src, sizeSrc);
	//*sizeDest  = sizeSrc;

	//fprintf(stderr, "compressed from %d to %d\n", sizeSrc, *sizeDest);
	_compSrcSize += sizeSrc;
	_compDestSize += *sizeDest;
	return dest;
}

static void decompressData(UInt8* dest, UInt8* src, lzo_uint sizeSrc, lzo_uintp sizeDest) {
	assert(src != NULL);
	assert(dest != NULL);
	assert(sizeSrc > 0);
	assert(sizeDest != NULL);

	int result = lzo1x_decompress(src, sizeSrc, dest, sizeDest, NULL);
	assert(result == LZO_E_OK);
	//memcpy(dest, src, sizeSrc);
	//*sizeDest = sizeSrc;

	//fprintf(stderr, "decompressed from %d to %d\n", sizeSrc, *sizeDest);
	MemPoolFree(src);
}

#if 0
UInt8* compressData(UInt8* src, uLong sizeSrc, uLong* sizeDest) {
	assert(src != NULL);
	assert(sizeSrc > 0);
	assert(sizeDest != NULL);

	*sizeDest = sizeSrc;
	//UInt8* dest = malloc(sizeSrc); // TODO: move away from malloc/free
	UInt8* dest = MemPoolAlloc(); //XXX need a specialized memory allocator
	*sizeDest = LZ_CompressFast(src, dest, sizeSrc, compressBuf);
#if 0
	int compStatus = compress(dest, sizeDest, src, sizeSrc);
	assert(compStatus == Z_OK);
	if (compStatus != Z_OK) {
		fprintf(stderr, "compress error!\n");
		exit(1);
	}
#endif
	//return realloc(dest, *sizeDest);
	return dest;
}

void decompressData(UInt8* dest, UInt8* src, uLong sizeSrc, uLong* sizeDest) {
	assert(src != NULL);
	assert(dest != NULL);
	assert(sizeSrc > 0);
	assert(sizeDest != NULL);
	LZ_Uncompress(src, dest, sizeSrc);
#if 0
	int compStatus = uncompress(dest, sizeDest, src, sizeSrc);
	assert(compStatus == Z_OK);
	if (compStatus != Z_OK) {
		fprintf(stderr, "decompress error!\n");
		exit(1);
	}
#endif
	MemPoolFree(src);
}
#endif

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

#if 0
UInt64 compressLTable(LTable* lTable) {
	if (lTable->code != 0xDEADBEEF) {
		fprintf(stderr, "LTable addr = 0x%llx\n", lTable);
		assert(0);
	}
	assert(lTable->code == 0xDEADBEEF);
	assert(lTable->isCompressed == 0);
	
	lTable->isCompressed = 1;
}

UInt64 decompressLTable(LTable* lTable) {
	if (lTable->code != 0xDEADBEEF) {
		fprintf(stderr, "LTable addr = 0x%llx\n", lTable);
		assert(0);
	}
	assert(lTable->code == 0xDEADBEEF);
	assert(lTable->isCompressed == 1);
	
	lTable->isCompressed = 0;

}
#endif
#if 1
// we'll assume you already GC'ed lTable... otherwise you are going to be
// doing useless work (i.e. compressing data that is out of date)
// Returns: number of bytes saved by compression
UInt64 compressLTable(LTable* lTable) {
	if (lTable->code != 0xDEADBEEF) {
		fprintf(stderr, "LTable addr = 0x%llx\n", lTable);
		assert(0);
	}
	assert(lTable->code == 0xDEADBEEF);
	if(lTable->isCompressed == 1) 
		return 0;

	TimeTable* tt1 = lTable->tArray[0];
	if(tt1 == NULL) 
		return 0;

	//fprintf(stderr,"compressing LTable (%p)\n",lTable);

	UInt64 compressionSavings = 0;
	lzo_uint srcLen = sizeof(Time)*TIMETABLE_SIZE/2; // XXX assumes 8 bytes
	lzo_uint compLen = 0;

	void* compressedData = compressData((UInt8*)tt1->array, srcLen, &compLen);
	Time* level0Array = tt1->array;
	tt1->array = compressedData;
	tt1->size = compLen;
	compressionSavings += (srcLen - compLen);

	// only for result checking
	//lTable->tArrayBackup[0] = level0Array;

	int i;
	Time* diffBuffer = MemPoolAlloc();
	for(i = 1; i < MAX_LEVEL; ++i) {
			
		// step 1: create/fill in time difference table
		TimeTable* tt2 = lTable->tArray[i];
		if(tt2 == NULL)
			break;

		lTable->tArrayBackup[i] = tt2->array;

		//fprintf(stderr,"compressing level %d\n",i);
		//printTimeTable(tt2);

		// for now, we'll always diff based on level 0
		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			diffBuffer[j] = level0Array[j] - tt2->array[j];
			//fprintf(stderr,"diffs[%d] = %llu\n",j,diffs[j]);
		}

		// step 2: compress diffs
		compressedData = compressData((UInt8*)diffBuffer, srcLen, &compLen);
		compressionSavings += (srcLen - compLen);
		tt2->size = compLen;

		// step 3: profit
		MemPoolFree(tt2->array); // XXX: comment this out if using tArrayBackup
		tt2->array = compressedData;
	}

	MemPoolFree(level0Array);  // XXX: comment this out if using tArrayBackup
	MemPoolFree(diffBuffer);

	lTable->isCompressed = 1;
	return compressionSavings;
}


UInt64 decompressLTable(LTable* lTable) {

	if (lTable->code != 0xDEADBEEF) {
		fprintf(stderr, "LTable addr = 0x%llx\n", lTable);
		assert(0);
	}
	assert(lTable->code == 0xDEADBEEF);
	if(lTable->isCompressed == 0) 
		return 0;

	UInt64 decompressionCost = 0;
	lzo_uint srcLen = sizeof(Time)*TIMETABLE_SIZE/2;
	lzo_uint uncompLen = srcLen;

	// for now, we'll always diff based on level 0
	TimeTable* tt1 = lTable->tArray[0];
	int compressedSize = tt1->size;

	Time* decompedArray = MemPoolAlloc();
	decompressData((UInt8*)decompedArray, (UInt8*)tt1->array, compressedSize, &uncompLen);

	tt1->array = decompedArray;
	decompressionCost += (srcLen - compressedSize);
	tt1->size = srcLen;

	//tArrayIsDiff(tt1->array, lTable->tArrayBackup[0]);

	int i;
	Time *diffBuffer = MemPoolAlloc();
	for(i = 1; i < MAX_LEVEL; ++i) {
		TimeTable* tt2 = lTable->tArray[i];
		if(tt2 == NULL) 
			break;

		// step 1: decompress time different table, 
		// the src buffer will be freed in decompressData
		uncompLen = srcLen;
		decompressData((UInt8*)diffBuffer, (UInt8*)tt2->array, tt2->size, &uncompLen);
		
		assert(srcLen == uncompLen);
		decompressionCost += (srcLen - tt2->size);

		// step 2: add diffs to base TimeTable
		tt2->array = MemPoolAlloc();
		tt2->size = srcLen;

		int j;
		for(j = 0; j < TIMETABLE_SIZE/2; ++j) {
			tt2->array[j] = tt1->array[j] - diffBuffer[j];
		}
		//tArrayIsDiff(tt2->array, lTable->tArrayBackup[i]);
	}
	MemPoolFree(diffBuffer);


	lTable->isCompressed = 0;
	return decompressionCost;
}
#endif


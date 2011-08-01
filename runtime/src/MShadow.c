#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "defs.h"
#include "log.h"
#include "debug.h"
//#include "table.h"
#include "GTable.h"
#include "kremlin_deque.h"
#include "hash_map.h"
#include "cregion.h"
#include "Vector.h"
#include "MShadow.h"

#define ALLOCATOR_SIZE (8ll * 1024 * 1024 * 1024 * 0 + 1)
#define DS_ALLOC_SIZE   100     // used for static data structures
#define MAX_SRC_TSA_VAL	6
#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

static GTable* gTable;




#ifdef EXTRA_STATS
UInt64 getReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version) {
    int level = inLevel - __kremlin_min_level;
    assert(entry != NULL);
    return (level >= 0 && entry->timeArrayLength > level && entry->readVersion[level] == version) ?
                    entry->readTime[level] : 0;
}

void updateReadTimestamp(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int level = inLevel - __kremlin_min_level;
    assert(entry != NULL);

    entry->readVersion[level] = version;
    entry->readTime[level] = timestamp;
}

void updateReadMemoryAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getReadTimestamp(entry, inLevel, version);

    if (prevTimestamp == 0LL) {
        regionInfo[region].readCnt++;
        //fprintf(stderr, "\t[load] addr = 0x%x level = %d version = %d timestamp = %d\n",
        //  entry, inLevel, version, timestamp);

        updateReadTimestamp(entry, inLevel, version, timestamp);
    }
}
    
// 1) update readCnt and writeCnt
// 2) update timestamp
void updateWriteMemoryAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].writeCnt++;
    }   
    //updateTimestamp(entry, inLevel, version, timestamp);
}

void updateReadMemoryLineAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getReadTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].readLineCnt++;
        //fprintf(stderr, "[line] addr = 0x%x level = %d version = %d timestamp = %d\n",
        //  entry, inLevel, version, timestamp);
        updateReadTimestamp(entry, inLevel, version, timestamp);

    }
}

void updateWriteMemoryLineAccess(TEntry* entry, UInt32 inLevel, UInt32 version, UInt64 timestamp) {
    int region = inLevel;
    //UInt64 startTime = regionInfo[region].start;
    UInt64 prevTimestamp = getTimestamp(entry, inLevel, version);
    if (prevTimestamp == 0LL) {
        regionInfo[region].writeLineCnt++;
    }
    TEntryUpdate(entry, inLevel, version, timestamp);
}
#endif

UInt MShadowInit() {
	GTableCreate(&gTable);	
}


UInt MShadowFinalize() {
	GTableDelete(&gTable);
}


Timestamp memGetTimestamp(Addr addr, Index index) {	
	Version version = versionGet(index);
	TEntry* entry = GTableGetTEntry(gTable, addr);
	Timestamp ts = TEntryGet(entry, index, version);
	return ts;
}

void memSetTimestamp(Timestamp time, Addr addr, Index index) {
	Version version = versionGet(index);
	TEntry* entry = GTableGetTEntry(gTable, addr);
	TEntryUpdate(entry, index, version, time);
}


#if 0
// Sat's version from here...
// move to table.c
void fillTSArray(TEntry* src_entry, TSArray* dest_tsa) {
	int fill_size = MIN(src_entry->timeArrayLength,numActiveLevels);

	// Fill in the data, taking into account version
	int i;
	for(i = 0; i < fill_size; ++i) {
    	dest_tsa->times[i] = (src_entry->version[i] == versions[i]) ?  src_entry->time[i] : 0;
	}

	// TODO: memset for this?
	for(i = fill_size; i < numActiveLevels; ++i) { dest_tsa->times[i] = 0; }
}

/*
 * Filles tsa array with timestamps associated with vreg
 * @param vreg			Virtual register number to get data from
 * @param tsa			TSArray to write TS data to
 */
// TODO: move to table.c
void getLocalTimes(UInt src_vreg, TSArray* tsa) {
	TEntry* entry = getLTEntry(src_vreg);

	fillTSArray(entry, tsa);
}

// TODO: move to GTable.c
void getGlobalTimes(Addr src_addr, TSArray* tsa) {
    TEntry* entry = GTableGetTEntry(gTable, src_addr);

	fillTSArray(entry, tsa);
}

void getCDTTSArray(TSArray* tsa) {
	int i;
	for(i = 0; i < numActiveLevels; ++i) {
		tsa->times[i] = (cdtHead->version[i] == versions[i]) ?  cdtHead->time[i] : 0;
	}
}

// precondition: numActiveLevels > 0
TSArray* getSrcTSArray(UInt idx) { 
	assert(idx <= MAX_SRC_TSA_VAL);

	// TODO: move this if to logRegionEntry for more efficiency
	if(src_arrays_size < numActiveLevels) {
		src_arrays = realloc(src_arrays,sizeof(TSArray*)*numActiveLevels);

		for( ; src_arrays_size < numActiveLevels; ++src_arrays_size) {
			src_arrays[src_arrays_size] = malloc(sizeof(TSArray)*MAX_SRC_TSA_VAL);

			int i;
			for(i = 0; i < MAX_SRC_TSA_VAL; ++i) {
				src_arrays[src_arrays_size][i].times =
					malloc(sizeof(Timestamp)*(src_arrays_size+1));
				src_arrays[src_arrays_size][i].size = src_arrays_size+1;
			}
		}
	}

	return &src_arrays[numActiveLevels-1][idx];
}

// precondition: numActiveLevels > 0
TSArray* getDestTSArray() { 
	// TODO: move this if to logRegionEntry for more efficiency
	if(dest_arrays_size < numActiveLevels) {
		dest_arrays = realloc(dest_arrays,sizeof(TSArray)*numActiveLevels);

		for( ; dest_arrays_size < numActiveLevels; ++dest_arrays_size) {
			dest_arrays[dest_arrays_size].times = 
				malloc(sizeof(Timestamp)*(dest_arrays_size+1));
			dest_arrays[dest_arrays_size].size = dest_arrays_size+1;
		}
	}

	return &dest_arrays[numActiveLevels-1];
}

// TODO: move to GTable.c
void updateTEntry(TEntry* entry, TSArray* src_tsa) {
    TEntryAllocAtLeastLevel(entry, 0);

	int i;
	for(i = 0; i < numActiveLevels; ++i) {
		entry->time[i] = src_tsa->times[i];
		regionInfo[i].cp = MAX(src_tsa->times[i],regionInfo[i].cp); // update CP

		entry->version[i] = versions[i];
	}
}

// TODO: move to table.c
void updateLocalTimes(UInt dest_vreg, TSArray* src_tsa) {
	TEntry* entry = getLTEntry(dest_vreg);

	updateTEntry(entry, src_tsa);
}

// TODO: move to GTable.c
void updateGlobalTimes(Addr dest_addr, TSArray* src_tsa) {
    TEntry* entry = GTableGetTEntry(gTable, dest_addr);

	updateTEntry(entry, src_tsa);
}

// TODO: move to GTable.c
void eraseGlobalTimes(Addr addr) {
	GTableDeleteTEntry(gTable, addr);
}

#endif

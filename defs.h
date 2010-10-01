//#define NDEBUG
#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


#ifndef _PYRPROF_DEF
#define _PYRPROF_DEF

// unless specified, we assume no debugging
#ifndef PYRPROF_DEBUG
#define PYRPROF_DEBUG	0
#endif

#ifndef DEBUGLEVEL
#define DEBUGLEVEL		0
#endif

// save the last visited BB number 
// for now we do not use it
// but if we need it later, define it 

//#define MANAGE_BB_INFO	

#define TRUE 1
#define FALSE 0

#ifndef MAX_REGION_LEVEL
#define MAX_REGION_LEVEL	20		
#endif

#ifndef MIN_REGION_LEVEL
#define MIN_REGION_LEVEL	0
#endif

#define LOAD_COST			4
#define STORE_COST			1

#ifndef MALLOC_TABLE_SIZE
#define MALLOC_TABLE_SIZE	10000
#endif

typedef unsigned long       UInt32;
typedef signed long         Int32;
typedef unsigned int		UInt;
typedef signed int          Int;
typedef unsigned long long  UInt64;
typedef signed long long    Int64;
typedef void*               Addr;
typedef FILE                File;

enum RegionType {RegionFunc, RegionLoop};

typedef struct _DataEntry {
    UInt32* version;
    UInt64* time;

} TEntry;


/*
    LocalTable:
        local table uses virtual register number as its key
*/
typedef struct _LocalTable {
    int             size;
    TEntry**     array;

} LTable;


typedef struct _GTableEntry {
	unsigned short used; // number of entries that are in use
    TEntry* array[0x4000];
} GEntry;

/*
    GlobalTable:
        global table is a hashtable with lower address as its primary key.
*/
typedef struct _GlobalTable {
    GEntry* array[0x10000];
} GTable;


typedef struct _MTableEntry {
	Addr start_addr;
	size_t size;
} MEntry;

/*
	MallocTable:
		malloc table is a table to track active mallocs
*/
typedef struct _MallocTable {
	int	size;
	MEntry* array[MALLOC_TABLE_SIZE];
} MTable;


typedef UInt    WorkTable;
typedef struct _RegionInfo {
    int         type;
    UInt        did;
    LTable      lTable;
    GTable      gTable;
    WorkTable   work;

} RegionInfo;


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

TEntry* allocTEntry(int size);
void freeTEntry(TEntry* entry);
LTable* allocLocalTable(int size);
void freeLocalTable(LTable* table);
TEntry* getLTEntry(UInt32 index);
TEntry* getGTEntry(Addr addr);
void initDataStructure(int regionLevel);
void finalizeDataStructure();
UInt32 getTEntrySize(void);


//UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version);
void updateTimestamp(TEntry* entry, UInt32 level, UInt32 version, UInt64 timestamp);

/* The following funcs are inserted by the critical path instrumentation pass */
void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest); 
void* logBinaryOpConst(UInt opCost, UInt src, UInt dest); 

void* logAssignment(UInt src, UInt dest);
void* logAssignmentConst(UInt dest); 

void* logInsertValue(UInt src, UInt dest); 
void* logInsertValueConst(UInt dest); 

void* logLoadInst(Addr src_addr, UInt dest); 
void* logStoreInst(UInt src, Addr dest_addr); 
void* logStoreInstConst(Addr dest_addr); 

void logMalloc(Addr addr, size_t size);
void logRealloc(Addr old_addr, Addr new_addr, size_t size);
void logFree(Addr addr);

void* logPhiNode1CD(UInt dest, UInt src, UInt cd); 
void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2); 
void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3); 
void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4); 

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4);

void logPhiNodeAddCondition(UInt dest, UInt src);

void prepareInvoke(UInt);
void invokeThrew(UInt);
void invokeOkay(UInt);

void addControlDep(UInt cond);
void removeControlDep();

void prepareCall(void);
void addReturnValueLink(UInt dest);
void logFuncReturn(UInt src); 
void logFuncReturnConst(void);

void linkArgToLocal(UInt src); 
void linkArgToConst(void);
void transferAndUnlinkArg(UInt dest); 

void* logLibraryCall(UInt cost, UInt dest, UInt num_in, ...); 

void logBBVisit(UInt bb_id); 

void* logInductionVar(UInt dest); 
void* logInductionVarDependence(UInt induct_var); 

void* logReductionVar(UInt opCost, UInt dest); 

/* The following functions are inserted by region instrumentation pass */
void initProfiler();
void deinitProfiler();

void turnOnProfiler();
void turnOffProfiler();

void logRegionEntry(UInt region_id, UInt region_type);
void logRegionExit(UInt region_id, UInt region_type);

void logLoopIteration();

// the following two functions are part of our plans for c++ support
void cppEntry();
void cppExit();


// tempoarary use only
// setup local table. 
void setupLocalTable(UInt maxVregNum);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif

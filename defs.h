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

#define PYRPROF_DEBUG	0
#define DEBUGLEVEL		0


// save the last visited BB number 
// for now we do not use it
// but if we need it later, define it 

//#define MANAGE_BB_INFO	

#define TRUE 1
#define FALSE 0
#define MAX_REGION_LEVEL		30		// acceptable max region level
#define MAX_LOG_REGION_LEVEL	1		// max region level logged

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
    TEntry* array[0x4000];
} GEntry;

/*
    GlobalTable:
        global table is a hashtable with lower address as its primary key.
*/
typedef struct _GloablTable {
    GEntry* array[0x10000];
} GTable;


typedef UInt    WorkTable;
typedef struct _RegionInfo {
    int         type;
    UInt        did;
    LTable      lTable;
    GTable      gTable;
    WorkTable   work;

} RegionInfo;


TEntry* allocTEntry(int size);
void freeTEntry(TEntry* entry);
LTable* allocLocalTable(int size);
void freeLocalTable(LTable* table);
TEntry* getLTEntry(UInt32 index);
TEntry* getGTEntry(Addr addr);
void initDataStructure(int regionLevel);
void finalizeDataStructure();


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

void logPhiNode(UInt dest, UInt src, UInt num_cont_dep, ...); 

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

void* logInductionVarDependence(UInt induct_var); 


/* The following functions are inserted by region instrumentation pass */
void initProfiler();
void deinitProfiler();

void logRegionEntry(UInt region_id, UInt region_type);
void logRegionExit(UInt region_id, UInt region_type);


// tempoarary use only
// setup local table. 
void setupLocalTable(UInt maxVregNum);

#endif

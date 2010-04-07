#include <assert.h>
#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


#ifndef _PYRPROF_DEF
#define _PYRPROF_DEF

#define TRUE 1
#define FALSE 0

typedef unsigned long       UInt32;
typedef signed long         Int32;
typedef unsigned int		UInt;
typedef signed int          Int;
typedef unsigned long long  UInt64;
typedef signed long long    Int64;
typedef void*               Addr;

enum RegionType {RegionLoop, RegionFunc};

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


TEntry* getLTEntry(UInt32 index);
TEntry* getGTEntry(Addr addr);
void initDataStructure(int size, int regionLevel);
void finalizeDataStructure();

UInt64 getTimestamp(TEntry* entry, UInt32 level, UInt32 version);
void updateTimestamp(TEntry* entry, UInt32 level, UInt32 version, UInt64 timestamp);

/* The following funcs are inserted by the critical path instrumentation pass */
void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest); 
void* logBinaryOpConst(UInt opCost, UInt src, UInt dest); 

void* logAssignment(UInt src, UInt dest);
void* logAssignmentConst(UInt dest); 

void* logInsertValue(UInt src, UInt dest); 
void* logInsertValueConst(UInt dest); 

void* logLoadInst(const void* src_addr, UInt dest); 
void* logStoreInst(UInt src, const void* dest_addr); 

void logPhiNode(UInt dest, UInt num_incoming_values, UInt num_t_inits, ...); 

void addControlDep(UInt cond);
void removeControlDep();

void addReturnValueLink(UInt dest);
void logFuncReturn(UInt src); 

void linkArgToLocal(UInt src); 
void linkArgToConst();
void transferAndUnlinkArg(UInt dest); 

UInt logLibraryCall(UInt cost, UInt dest, UInt num_in, ...); 

void logBBVisit(UInt bb_id); 


/* The following functions are inserted by region instrumentation pass */
void initProfiler();
void deinitProfiler();

void logRegionEntry(UInt region_id, UInt region_type);
void logRegionExit(UInt region_id, UInt region_type);






/* The following functions are deprecated. Don't bother implementing them */
void linkInitToCondition(const void* rhs, const void* lhs);
UInt logInductionVarDependence(const void* induct_var); 

UInt logOutputToConsole(UInt id, UInt bb_id, int num_out, ...); 
UInt logInputFromConsole(UInt id, UInt bb_id, int num_in, ...); 

void stackVariableAlloc(UInt bb_id, const void* address); 
void stackVariableDealloc(UInt bb_id, const void* address); 

void linkAddrToArgName(UInt bb_id, const void* address, char* argname); 

void setReturnTimestampConst();
void setReturnTimestamp(UInt bb_id, const void* retval);
void getReturnTimestamp(UInt id, UInt bb_id, const void* address);

void onBasicBlockEntry(UInt bb_id);

void openOutputFile();
void closeOutputFile();

void linkInit(UInt cond);
void removeInit();

void printProfileData(void);

#endif

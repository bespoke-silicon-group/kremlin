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

typedef unsigned int        UInt;
typedef signed int          Int;
typedef unsigned long long  UInt64;
typedef signed long long    Int64;
typedef void*               Addr;

enum RegionType {RegionLoop, RegionFunc};

typedef struct _DataEntry {
    UInt32* version;
    UInt64* time;

} DataEntry;


/*
    LocalTable:
        local table uses virtual register number as its key
*/
typedef struct _LocalTable {
    int             size;
    DataEntry*      array;

} LTable;


typedef struct _GTableEntry {
    DataEntry array[0x4000];
} GEntry;

/*
    GlobalTable:
        global table is a hashtable with lower address as its primary key.
*/
typedef struct _GloablTable {
    GEntry array[0x10000];
} GTable;


typedef UInt    WorkTable;
typedef struct _RegionInfo {
    int         type;
    UInt        did;
    LTable      lTable;
    GTable      gTable;
    WorkTable   work;

} RegionInfo;

LTable* allocLocalTable(int size, int depth);
void    freeLocalTable(LTable* table);
void    updateLocalTime(LTable* table, int key, UInt64 timestamp);
UInt64  getLocalTime(LTable* table, int key);

GTable* allocGlobalTable(void);
void    freeGlobalTable(GTable* table);
GTEntry* getGTEntry(GTable* table, Addr addr);
UInt64  getGlobalTime(GTable* table, Addr addr);
void    updateGlobalTime(GTable* table, Addr addr, UInt64 timestamp);

/* The following funcs are inserted by the critical path instrumentation pass */
void* logBinaryOp(int op_cost, unsigned int src0, unsigned int src1, unsigned int dest); 
void* logBinaryOpConst(int op_cost, unsigned int src, unsigned int dest); 

void* logAssignment(unsigned int src, unsigned int dest);
void* logAssignmentConst(unsigned int dest); 

void* logInsertValue(unsigned int src_addr, unsigned int dst_addr); 
void* logInsertValueConst(unsigned int dst_addr); 

void* logLoadInst(const void* src_addr, unsigned int dest); 
void* logStoreInst(unsigned int src, const void* dest_addr); 

void logPhiNode(unsigned int dest, unsigned int num_incoming_values, unsigned int num_t_inits, ...); 

void addControlDep(unsigned int cond);
void removeControlDep();

void addReturnValueLink(unsigned int dest);
void logFuncReturn(unsigned int src); 

void linkArgToLocal(unsigned int src); 
void linkArgToConst();
void transferAndUnlinkArg(unsigned int dest); 

unsigned int logLibraryCall(unsigned int cost, unsigned int dest, unsigned int num_in, ...); 

void logBBVisit(unsigned int bb_id); 


/* The following functions are inserted by region instrumentation pass */
void initProfiler();
void deinitProfiler();

void logRegionEntry(unsigned int region_id, unsigned int region_type);
void logRegionExit(unsigned int region_id, unsigned int region_type);






/* The following functions are deprecated. Don't bother implementing them */
void linkInitToCondition(const void* rhs, const void* lhs);
unsigned int logInductionVarDependence(const void* induct_var); 

unsigned int logOutputToConsole(unsigned int id, unsigned int bb_id, int num_out, ...); 
unsigned int logInputFromConsole(unsigned int id, unsigned int bb_id, int num_in, ...); 

void stackVariableAlloc(unsigned int bb_id, const void* address); 
void stackVariableDealloc(unsigned int bb_id, const void* address); 

void linkAddrToArgName(unsigned int bb_id, const void* address, char* argname); 

void setReturnTimestampConst();
void setReturnTimestamp(unsigned int bb_id, const void* retval);
void getReturnTimestamp(unsigned int id, unsigned int bb_id, const void* address);

void onBasicBlockEntry(unsigned int bb_id);

void openOutputFile();
void closeOutputFile();

void linkInit(unsigned int cond);
void removeInit();

void printProfileData(void);

#endif

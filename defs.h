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
#define PYRPROF_DEBUG   0
#endif

#ifndef DEBUGLEVEL
#define DEBUGLEVEL      0
#endif

// save the last visited BB number 
// for now we do not use it
// but if we need it later, define it 

//#define MANAGE_BB_INFO    

#define TRUE 1
#define FALSE 0

#ifndef MAX_REGION_LEVEL
#define MAX_REGION_LEVEL    20      
#endif

#ifndef MIN_REGION_LEVEL
#define MIN_REGION_LEVEL    0
#endif

#define LOAD_COST           4
#define STORE_COST          1

typedef unsigned long       UInt32;
typedef signed long         Int32;
typedef unsigned int        UInt;
typedef signed int          Int;
typedef unsigned long long  UInt64;
typedef signed long long    Int64;
typedef void*               Addr;
typedef FILE                File;

enum RegionType {RegionFunc, RegionLoop};

#endif

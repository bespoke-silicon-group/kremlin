#ifndef _KTYPES_H
#define _KTYPES_H

#include <limits.h>
#include <stdarg.h> /* for variable length args */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>


#define TRUE 1
#define FALSE 0

typedef uint16_t 	       	UInt16;
typedef uint32_t 	       	UInt32;
typedef int32_t         	Int32;
typedef uint32_t			UInt;
typedef uint8_t				UInt8;
typedef signed int          Int;
typedef uint64_t			UInt64;
typedef int64_t				Int64;
typedef UInt32 				Bool;
typedef void*               Addr;
typedef FILE                File;
typedef UInt64 				Timestamp;
typedef UInt64 				Time;
typedef UInt64 				Version;
typedef UInt32 				Level;
typedef UInt32 				Index;
typedef UInt 				Reg;
typedef UInt64				SID; 	// static region ID
typedef UInt64				CID;	// callsite ID


typedef enum RegionType {RegionFunc, RegionLoop, RegionLoopBody} RegionType;

#endif

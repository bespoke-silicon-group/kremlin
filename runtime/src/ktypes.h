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

#define KREM_BINOP 0
#define KREM_REGION_ENTRY 1
#define KREM_REGION_EXIT 2
#define KREM_ASSIGN_CONST 3
#define KREM_LOAD 4
#define KREM_STORE 5
#define KREM_ARVL 6
#define KREM_FUNC_RETURN 7
#define KREM_PHI 8
#define KREM_CD_TO_PHI 9
#define KREM_ADD_CD 10
#define KREM_REMOVE_CD 11

#endif

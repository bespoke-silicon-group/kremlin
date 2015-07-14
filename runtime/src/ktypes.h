#ifndef _KTYPES_H
#define _KTYPES_H

#include <cstdint> // for uint16_t, etc.

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::int32_t;
using std::uint64_t;
using std::int64_t;

typedef void*               Addr;
typedef uint64_t 			Timestamp;
typedef uint64_t 			Time;
typedef uint64_t 			Version;
typedef uint32_t 			Level;
typedef uint32_t 			Index;
typedef uint32_t			Reg;
typedef uint64_t			SID; 	// static region ID
typedef uint64_t			CID;	// callsite ID


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
#define KREM_TS 12
#define KREM_LINK_ARG 13
#define KREM_UNLINK_ARG 14
#define KREM_PREP_CALL 15
#define KREM_PREP_REG_TABLE 16
#define KREM_REDUCTION 17
#define KREM_INDUCTION 18

#endif

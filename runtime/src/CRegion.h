#ifndef _CREGION_H_
#define _CREGION_H_

#include <vector>
#include "ktypes.h"

typedef struct _cstat_t CStat;
typedef struct _cnode_t CNode;
typedef struct _r_tree_t CTree;
typedef struct _cstack_item_t CItem;

// CNode types:
// NORMAL - summarizing non-recursive region
// R_INIT - recursion init node
// R_SINK - recursion sink node that connects to a R_INIT

enum _cnode_type {NORMAL, R_INIT, R_SINK};
typedef enum _cnode_type CNodeType;

struct _cstat_t {
	UInt64 totalWork;
	double minSP;
	double maxSP;

	UInt64 spWork;
	UInt64 tpWork;

	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 loadCnt;
	UInt64 storeCnt;
	
	UInt64 totalIterCount;
	UInt64 minIterCount;
	UInt64 maxIterCount;

	UInt64 numInstance;
};




struct _cnode_t {
	// identity
	CNodeType type;
	RegionType rType;
	UInt64 id;
	UInt64 sid;
	UInt64 cid;
	UInt64 numInstance;
	UInt64 isDoall;

	// for debugging 
	UInt32 code;

	// change to type?
	Bool   isRecursive;

	// contents
	// add more pointers based on type?
	std::vector<CStat*> stats;
	int curr_stat_index;
	CStat* statStart;
	CStat* stat;

	// management of tree
	CNode* parent;
	std::vector<CNode*> children;

	CTree* tree; // for linking a CTree
	CNode* recursion;
};

// FIXME: is this necessary anymore?
struct _cstack_item_t {
	CNode* node;
};

struct _r_tree_t {
	UInt64 id;
	int maxDepth;
	int currentDepth;
	CNode* root;
	CNode* parent;

	CItem* stackTop;
}; 

typedef struct _RegionField_t {
	UInt64 work;
	UInt64 cp;
	UInt64 callSite;
	UInt64 spWork;
	UInt64 isDoall;
	UInt64 childCnt;
#ifdef EXTRA_STATS
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 writeLineCnt;
	UInt64 loadCnt;
	UInt64 storeCnt;
#endif
} RegionField;


void CRegionInit();
void CRegionDeinit(const char* file);
void CRegionEnter(SID sid, CID callSite, RegionType type);
void CRegionExit(RegionField* info);

#endif

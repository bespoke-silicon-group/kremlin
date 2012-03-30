#ifndef _CREGION_H_
#define _CREGION_H_

#include "ktypes.h"

typedef struct _cstat_t CStat;
typedef struct _cnode_t CNode;
typedef struct _r_tree_t CTree;
typedef struct _cstack_item_t CItem;

// CNode types:
// NORMAL - summarizing non-recursive region
// EXT_R  - entry to a recursive region tree
// SELF_R - leaf node in a recursive region tree
//          pointing to the root of the current tree
//          converted from NORMAL when a recursion is detected

enum _cnode_type {NORMAL, EXT_R, SELF_R};
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
	
	UInt64 isDoall;

	// double linked-list for
	// efficient accounting in recursion
	UInt64 index;
	CStat* next;	
	CStat* prev;	
};




struct _cnode_t {
	// identity
	CNodeType type;
	UInt64 id;
	UInt64 sid;
	UInt64 cid;
	UInt64 numInstance;

	UInt64 totalChildCount;
	UInt64 minChildCount;
	UInt64 maxChildCount;

	// for debugging 
	UInt32 code;

	// change to type?
	Bool   isRecursive;

	// contents
	// add more pointers based on type?
	CStat* stat;

	// management of tree
	CNode* parent;
	CNode* firstChild;
	CNode* next; // for siblings	
	CTree* tree; // for linking a CTree
	UInt64 childrenSize;
};

struct _cstack_item_t {
	CNode* node;
	// doubly linked list
	CItem* prev;
	CItem* next;	
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
void CRegionDeinit(char* file);
void CRegionEnter(SID sid, CID callSite);
void CRegionLeave(RegionField* info);

#endif

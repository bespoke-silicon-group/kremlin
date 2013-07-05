#ifndef _CNODE_H_
#define _CNODE_H_

#include <vector>
#include "CRegion.h"

#define DEBUG_CREGION	3

// CNode types:
// NORMAL - summarizing non-recursive region
// R_INIT - recursion init node
// R_SINK - recursion sink node that connects to a R_INIT
enum _cnode_type {NORMAL, R_INIT, R_SINK};
typedef enum _cnode_type CNodeType;

class CNode {
public:
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

	// statistics for node
	std::vector<CStat*> stats;
	int curr_stat_index;

	// management of tree
	CNode* parent;
	std::vector<CNode*> children;

	CTree* tree; // for linking a CTree
	CNode* recursion;

	CNode* findChild(UInt64 sid, UInt64 callSite);
	void   CNodeAttach(CStat* region);
	void   linkChild(CNode* child); 
	void   update(RegionField* info);
	void   replaceChild(CNode* old_child, CNode* new_child);
	char*  toString(); // make static to handle null case?


	//CNodeType getType() { return type; }

#if 0
	bool isRNode() { return (this->tree != NULL); }
	bool isSelfRNode(CTree* tree);
	static void   convertToSelfRNode(CTree* tree);
	static CNode* createSelfRNode(SID sid, CID cid, CTree* childTree); 
#endif

	static CNode* create(SID sid, CID cid, RegionType type); 
	//static CNode* createExtRNode(SID sid, CID cid, CTree* childTree); 
	
private:
	void updateCurrentCStat(RegionField* info);
	static UInt64 allocId();
};

#endif

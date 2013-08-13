#ifndef _CNODE_H_
#define _CNODE_H_

#include <vector>
#include "PoolAllocator.hpp"
#include "CRegion.h"
//#include "CStat.h"

#define DEBUG_CREGION	3

// CNode types:
// NORMAL - summarizing non-recursive region
// R_INIT - recursion init node
// R_SINK - recursion sink node that connects to a R_INIT
enum _cnode_type {NORMAL, R_INIT, R_SINK};
typedef enum _cnode_type CNodeType;

class CTree;
class CStat;

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
	std::vector<CStat*, MPoolLib::PoolAllocator<CStat*> > stats;
	int curr_stat_index;

	// management of tree
	CNode* parent;
	std::vector<CNode*, MPoolLib::PoolAllocator<CNode*> > children;

	CTree* tree; // for linking a CTree
	CNode* recursion;

	CNode(SID static_id, CID callsite_id, RegionType type);
	~CNode();

	CNode* getChild(UInt64 sid, UInt64 callSite);
	void   addChild(CNode* child); 

	void   CNodeAttach(CStat* region);
	void   statForward();
	void   statBackward();
	void   updateStats(RegionField* info);
	void   replaceChild(CNode* old_child, CNode* new_child);
	const char*  toString(); // make static to handle null case?

	unsigned getStatSize() { return stats.size(); }
	unsigned getNumChildren() { return children.size(); }

	/*!
	 * Returns an ancestor with the same static region ID. If no such ancestor
	 * is found, returns NULL.
	 *
	 * @return The ancestor with the same static ID as this node; NULL if none exist.
	 */
	CNode* findAncestorBySid();

	/*!
	 * Checks if this node is a recursive instance of an already existing node.
	 * If this node is a recursive node, we set its type and its
	 * ancestor (of which it is a recursive instance) and creates a link from
	 * this node to this ancestor.
	 */
	void handleRecursion(); 

	static void* operator new(size_t size);
	static void operator delete(void* ptr);
	
private:
	void updateCurrentCStat(RegionField* info);
	static UInt64 allocId();

#if 0
	static CNode* createExtRNode(SID sid, CID cid, CTree* childTree); 
	bool isRNode() { return (this->tree != NULL); }
	bool isSelfRNode(CTree* tree);
	static void   convertToSelfRNode(CTree* tree);
	static CNode* createSelfRNode(SID sid, CID cid, CTree* childTree); 
#endif
};

#endif

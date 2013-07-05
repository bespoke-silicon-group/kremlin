
#include <stdlib.h>
#include <stack>

#include "config.h"
#include "kremlin.h"
#include "MemMapAllocator.h"
#include "CRegion.h"

#define DEBUG_CREGION	3

/**
 * CPosition tracks the current region tree / node
 */
typedef struct _CPosition {
	CTree* tree;		
	CNode* node;
	
} CPosition;


/*** local functions ***/
static void   CPositionInit();
static void   CPositionDeinit();
static CTree* CPositionGetTree();
static CNode* CPositionGetNode();
static void   CPositionSet(CTree* tree, CNode* node);
static void   CPositionSetTree(CTree*);
static void   CPositionSetNode(CNode*);
static char*  CPositionToStr();

static void   CRegionPush(CNode* node);
static CNode* CRegionPop();

static CStat* CStatCreate(int index);
static void   CStatDelete(CStat* region);

static CNode* CNodeCreate(SID sid, CID cid, RegionType type); 
static CNode* CNodeCreateExtRNode(SID sid, CID cid, CTree* childTree); 
static void   CNodeConvertToSelfRNode(CNode* node, CTree* tree);
//static CNode* CNodeCreateSelfRNode(SID sid, CID cid, CTree* childTree); 
static Bool   CNodeIsRNode(CNode* node); 
static CNode* CNodeFindChild(CNode* node, UInt64 sid, UInt64 callSite);
static void   CNodeAttach(CNode* node, CStat* region);
static void   CNodeLink(CNode* parent, CNode* child); 
static void   CNodeUpdate(CNode* node, RegionField* info);
static void   CNodeReplaceChild(CNode*, CNode*, CNode*);
static char*  CNodeToString(CNode* node); 
static Bool   CNodeIsSelfRNode(CTree* tree, CNode* node);
static CNodeType CNodeGetType(CNode* node);

static void CNodeStatForward(CNode* current);
static void CNodeStatBackward(CNode* current);

static CTree* CTreeCreate(CNode* root); 
static void   CTreeDelete(CTree* tree);
static CNode* CTreeFindAncestorBySid(CTree* tree, CNode* child);
static CTree* CTreeConvertFromSubTree(CNode* root, CNode* recurseNode); 
static void   CTreeEnterNode(CTree* tree, CNode* node);
static CNode* CTreeExitNode(CTree* tree);
static void   CTreeIncDepth(CTree* tree);
static void   CTreeDecDepth(CTree* tree);
static void   CTreeHandleRecursion(CTree* tree, CNode* child); 

static void emit(const char* file);
static void emitRegion(FILE* fp, CNode* node, UInt level);

static void* CRegionMemAlloc(int size, int site) {
	void* ret = MemPoolAllocSmall(size);
	return ret;
}

static void CRegionMemFree(void* addr, int size, int site) {
	MemPoolFreeSmall(addr, size);
}



/******************************** 
 * Public Functions
 *********************************/

void CRegionInit() {
	CNode* root = CNodeCreate(0, 0, RegionFunc); // dummy root node
	CTree* tree = CTreeCreate(root);
	CPositionInit();
	CPositionSetTree(tree);
	assert(root != NULL);
	CPositionSetNode(root);
}

void CRegionDeinit(const char* file) {
	assert(CRegionPop() == NULL);
	emit(file);
}

void printPosition() {
	MSG(DEBUG_CREGION, "Curr %s Node: %s\n", CPositionToStr(), CNodeToString(CPositionGetNode()));
}

void CRegionEnter(SID sid, CID cid, RegionType type) {
	if (KConfigGetCRegionSupport() == FALSE)
		return;

	CNode* parent = CPositionGetNode();
	CNode* child = NULL;

	// corner case: no graph exists 
#if 0
	if (parent == NULL) {
		child = CNodeCreate(sid, cid);
		CPositionSetNode(child);
		CRegionPush(child);
		MSG(0, "CRegionEnter: sid: root -> 0x%llx, callSite: 0x%llx\n", 
			sid, cid);
		return;
	}
#endif
	
	assert(parent != NULL);
	MSG(DEBUG_CREGION, "CRegionEnter: sid: 0x%llx -> 0x%llx, callSite: 0x%llx\n", 
		parent->sid, sid, cid);

	// make sure a child node exist
	child = CNodeFindChild(parent, sid, cid);
	if (child == NULL) {
		// case 3) step a - new node required
		child = CNodeCreate(sid, cid, type);
		CNodeLink(parent, child);
		if (KConfigGetRSummarySupport())
			CTreeHandleRecursion(CPositionGetTree(), child);
	} 

	CNodeStatForward(child);

	assert(child != NULL);
	// set position, push the current region to the current tree
	switch (child->type) {
	case R_INIT:
		CPositionSetNode(child);
		break;
	case R_SINK:
		assert(child->recursion != NULL);
		CPositionSetNode(child->recursion);
		CNodeStatForward(child->recursion);
		break;
	case NORMAL:
		CPositionSetNode(child);
		break;
	}
	CRegionPush(child);
	printPosition();

	MSG(DEBUG_CREGION, "CRegionEnter: End\n"); 
}

// at the end of a region execution,
// pass the region exec info.
// update the passed info and 
// set current pointer to one level higher
void CRegionExit(RegionField* info) {
	if (KConfigGetCRegionSupport() == FALSE)
		return;

	MSG(DEBUG_CREGION, "CRegionLeave: Begin\n"); 
	// don't update if we didn't give it any info
	// this happens when we are out of range for logging
	CNode* current = CPositionGetNode();
	CNode* popped = CRegionPop();
	assert(popped != NULL);
	assert(current != NULL);

	MSG(DEBUG_CREGION, "Curr %s Node: %s\n", CPositionToStr(), CNodeToString(current));
#if 0
	if (info != NULL) {
		assert(current != NULL);
		CNodeUpdate(current, info);
	}
#endif

	assert(info != NULL);
	CNodeUpdate(current, info);

	assert(current->curr_stat_index != -1);
	MSG(DEBUG_CREGION, "Update Node 0 - ID: %d Page: %d\n", current->id, current->curr_stat_index);
	CNodeStatBackward(current);
	assert(current->parent != NULL);

	if (current->type == R_INIT) {
		CPositionSetNode(popped->parent);	

	} else {
		CPositionSetNode(current->parent);	
	}

	if (popped->type == R_SINK) {
		CNodeUpdate(popped, info);
		MSG(DEBUG_CREGION, "Update Node 1 - ID: %d Page: %d\n", popped->id, popped->curr_stat_index);
		CNodeStatBackward(popped);
	} 
	printPosition();
	MSG(DEBUG_CREGION, "CRegionLeave: End \n"); 
}

/**
 * CRegionPush / CRegionPop
 *
 */

static std::stack<CNode*> c_region_stack;

static void CRegionPush(CNode* node) {
	MSG(DEBUG_CREGION, "CRegionPush: ");

	c_region_stack.push(node);

	MSG(DEBUG_CREGION, "%s\n", CNodeToString(node));
}

static CNode* CRegionPop() {
	MSG(DEBUG_CREGION, "CRegionPop: ");

	if (c_region_stack.empty()) {
		return NULL;
	}

	CNode* ret = c_region_stack.top();
	c_region_stack.pop();
	MSG(DEBUG_CREGION, "%s\n", CNodeToString(ret));

	return ret;
}



/******************************** 
 * CPosition Management 
 *********************************/
static CPosition _curPosition;

static void CPositionInit() {
	_curPosition.node = NULL;
}

static void CPositionDeinit() {
}

#if 0
static void CPositionUpdateEnter(CNode* node) {
	CTreeEnterNode(tree, node);
}

static void CPositionUpdateExit() {
	
}
#endif


static CTree* CPositionGetTree() {
	return _curPosition.tree;
}

static CNode* CPositionGetNode() {
	return _curPosition.node;
}

#if 0
static void CPositionSet(CTree* tree, CNode* node) {
	_curPosition.tree = tree;
	_curPosition.node = node;

}
#endif


static void CPositionSetNode(CNode* node) {
	assert(node != NULL);
	_curPosition.node = node;

}

static void CPositionSetTree(CTree* tree) {
	_curPosition.tree = tree;
}


static char _bufCur[16];
static char* CPositionToStr() {
	CNode* node = CPositionGetNode();
	UInt64 nodeId = (node == NULL) ? 0 : node->id;
	sprintf(_bufCur, "<%5d>", nodeId);
	return _bufCur;
}



/*****************************
 * CStat Related Routines
 *****************************/


static CStat* CStatCreate(int index) {
	CStat* ret = (CStat*)CRegionMemAlloc(sizeof(CStat), 1);
	ret->totalWork = 0;
	ret->tpWork = 0;
	ret->spWork = 0;
	ret->minSP = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxSP = 0;
	ret->readCnt = 0;
	ret->writeCnt = 0;
	ret->loadCnt = 0;
	ret->storeCnt = 0;

	// iteration info	
	ret->totalIterCount = 0;
	ret->minIterCount = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxIterCount = 0;

	ret->numInstance = 0;

	return ret;
}

static void CStatDelete(CStat* stat) {
	CRegionMemFree(stat, sizeof(CStat), 1); 
}


static void CStatUpdate(CNode* node, RegionField* info) {
	assert(node->curr_stat_index != -1);
	assert(info != NULL);

	CStat* stat = node->stats[node->curr_stat_index];

	MSG(DEBUG_CREGION, "CStatUpdate: work = %d, spWork = %d\n", info->work, info->spWork);
	
	double sp = (double)info->work / (double)info->spWork;
	stat->numInstance++;
	if (stat->minSP > sp) stat->minSP = sp;
	if (stat->maxSP < sp) stat->maxSP = sp;
	stat->totalWork += info->work;
	stat->tpWork += info->cp;
	stat->spWork += info->spWork;

	stat->totalIterCount += info->childCnt;
	if (stat->minIterCount > info->childCnt) 
		stat->minIterCount = info->childCnt;
	if (stat->maxIterCount < info->childCnt) 
		stat->maxIterCount = info->childCnt;


#ifdef EXTRA_STATS
	stat->readCnt += info->readCnt;
	stat->writeCnt += info->writeCnt;
	stat->loadCnt += info->loadCnt;
	stat->storeCnt += info->storeCnt;
#endif
}

/****************************
 * CNode Related Routines 
 ***************************/

static void CNodeStatForward(CNode* node) {
	assert(node != NULL);
	int stat_index = ++(node->curr_stat_index);

	MSG(DEBUG_CREGION, "CStatForward id %d to page %d\n", node->id, stat_index);

	// FIXME: it appears as though if and else-if can be combined
	if (node->stats.size() == 0) {
		assert(stat_index == 0); // FIXME: is this correct assumption?
		CStat* new_stat = CStatCreate(0);
		node->stats.push_back(new_stat);
	}

	else if (stat_index >= node->stats.size()) {
		CStat* new_stat = CStatCreate(stat_index);
		node->stats.push_back(new_stat);
	}
}

static void CNodeStatBackward(CNode* node) {
	assert(node != NULL);
	assert(node->curr_stat_index != -1);
	MSG(DEBUG_CREGION, "CStatBackward id %d from page %d\n", node->id, node->curr_stat_index);
	--(node->curr_stat_index);
}

static void CNodeUpdate(CNode* node, RegionField* info) {
	assert(info != NULL);
	MSG(DEBUG_CREGION, "CRegionUpdate: cid(0x%lx), work(0x%lx), cp(%lx), spWork(%lx)\n", 
			info->callSite, info->work, info->cp, info->spWork);
	MSG(DEBUG_CREGION, "current region: id(0x%lx), sid(0x%lx), cid(0x%lx)\n", 
			node->id, node->sid, node->cid);

	//assert(node->cid == info->callSite);
	assert(node->numInstance >= 0);

	// handle P bit for DOALL identification
	if (info->isDoall == 0)
		node->isDoall = 0;

	node->numInstance++;
	CStatUpdate(node, info);
}

static CNode* CNodeFindChild(CNode* node, UInt64 sid, UInt64 callSite) {
	//fprintf(stderr, "looking for sid : 0x%llx, callSite: 0x%llx\n", sid, callSite);
	for (unsigned i = 0; i < node->children.size(); ++i) {
		CNode* child = node->children[i];
		//fprintf(stderr, "\tcandidate sid : 0x%llx, callSite: 0x%llx\n", child->sid, child->cid);
		if ( child->sid == sid
			&& (child->rType != RegionFunc || child->cid == callSite)
		   ) {
			return child;
		}
	}

	//fprintf(stderr, "\tnot found, creating one..\n");
	return NULL;
}

static void CNodeLink(CNode* parent, CNode* child) {
	assert(parent != NULL);
	parent->children.push_back(child);
	child->parent = parent;
}

static UInt64 lastId = 0;
static UInt64 CNodeAllocId() { return ++lastId; }


static CNode* CNodeCreate(SID sid, CID cid, RegionType type) {
	CNode* ret = (CNode*)CRegionMemAlloc(sizeof(CNode), 2);
	MSG(DEBUG_CREGION, "CNode: created CNode at 0x%x\n", ret);

	// basic info
	ret->parent = NULL;
	ret->type = NORMAL;
	ret->rType = type;
	ret->id = CNodeAllocId();
	ret->sid = sid;
	ret->cid = cid;
	ret->recursion = NULL;
	ret->numInstance = 0;
	ret->isDoall = 1;

	// debug info
	ret->code = 0xDEADBEEF;

	// stat
	ret->curr_stat_index = -1;

	return ret;
}

#if 0
static CNode* CNodeCreateExtRNode(SID sid, CID cid, CTree* childTree) {
	CNode* ret = CNodeCreate(sid, cid);
	ret->type = EXT_R;
	ret->tree = childTree;
	childTree->parent = ret;
	return ret;
}; 

static void CNodeConvertToSelfRNode(CNode* node, CTree* tree) {
	node->tree = tree;
	node->type = SELF_R;
}
#endif

static Bool CNodeIsRNode(CNode* node) {
	return (node->tree != NULL);
}

static Bool CNodeIsSelfRNode(CTree* tree, CNode* node) {
	if (!CNodeIsRNode(node))
		return FALSE;

	return (node->tree == tree);
}


// side effect: oldRegion's parent is set to NULL
static void CNodeReplaceChild(CNode* parent, CNode* old_region, CNode* new_region) {
	old_region->parent = NULL;

	for (unsigned i = 0; i < parent->children.size(); ++i) {
		CNode* child = parent->children[i];
		if (child == old_region) {
			parent->children[i] = new_region;
			return;
		}
	}

	assert(0);
	return;
}


/**
 * Convert a subgraph into a recursive node
 */

static char _buf[256];
static char* _strType[] = {"NORM", "RINIT", "RSINK"};

static char* CNodeToString(CNode* node) {
	if (node == NULL) {
		sprintf(_buf, "NULL"); 
		return _buf;
	}

	UInt64 parentId = (node->parent == NULL) ? 0 : node->parent->id;
	UInt64 childId = (node->children.empty()) ? 0 : node->children[0]->id;
	sprintf(_buf, "id: %d, type: %5s, parent: %d, firstChild: %d, sid: %llx", 
		node->id, _strType[node->type], parentId, childId, node->sid);
	return _buf;
}

// TODO: remove this one-line function?
static int CNodeGetStatSize(CNode* node) {
	return node->stats.size();
}



/******************************************
 * CTree Related
 ******************************************/

static UInt64 lastTreeId = 0;
static UInt64 CTreeAllocId() { return ++lastTreeId; }

static Bool CTreeIsRoot(CTree* tree) {
	return tree->parent == NULL;
}

static CNode* CTreeGetParent(CTree* tree) {
	return tree->parent;
}

static CNode* CTreeGetRoot(CTree* tree) {
	return tree->root;
}

static void CTreeIncDepth(CTree* tree) {
	tree->currentDepth++;
	if (tree->currentDepth > tree->maxDepth)
		tree->maxDepth = tree->currentDepth;
}

static void CTreeDecDepth(CTree* tree) {
	tree->currentDepth--;
}

static UInt CTreeGetMaxDepth(CTree* tree) {
	return tree->maxDepth;
}

static UInt CTreeGetDepth(CTree* tree) {
	return tree->currentDepth;
}

static CTree* CTreeCreate(CNode* root) {
	CTree* ret = (CTree*)CRegionMemAlloc(sizeof(CTree), 3);
	ret->id = CTreeAllocId();
	ret->maxDepth = 0;
	ret->currentDepth = 0;
	ret->parent = NULL;
	ret->root = root;
	return ret;
}


static void CTreeDelete(CTree* tree) {
	// first, traverse and free every node in the tree
	// TODO Here

	// second, free the tree storage
	MemPoolFreeSmall(tree, sizeof(CTree));
}

static CNode* CTreeFindAncestorBySid(CTree* tree, CNode* child) {
	MSG(DEBUG_CREGION, "CTreeFindAncestor: sid: 0x%llx....", child->sid);
	SID sid = child->sid;
	CNode* node = child->parent;
	
	while (node != NULL) {
		if (node->sid == sid) {
			return node;
		}

		node = node->parent;
	}
	return NULL;
}

/**
 * Create a CTree from a subtree
 */
static CTree* CTreeConvertFromSubTree(CNode* root, CNode* recurseNode) {
	CTree* ret = CTreeCreate(root);
	recurseNode->tree = ret;
	return ret;
}	

static void CTreeHandleRecursion(CTree* tree, CNode* child) {
	// detect a recursion with a new node
	// - find an ancestor where ancestor.sid == child.sid
	// - case a) no ancestor found - no recursion
	// - case b) recursion to the root node: self recursion
	//			 transform child to RNode
	// - case c) recursion to a non-root node: a new tree needed
	//	       - create a CTree from a subtree starting from the ancestor
	//         - set current tree and node appropriately

	CNode* ancestor = CTreeFindAncestorBySid(tree, child);

	if (ancestor == NULL) {
		return;
#if 0
	} else if (ancestor == tree->root) {
		CNodeConvertToSelfRNode(child, tree);
		return;
#endif

	} else {
		assert(ancestor->parent != NULL);
		//CTree* rTree = CTreeConvertFromSubTree(ancestor, child);	
		//CNode* rNode = CNodeCreateExtRNode(ancestor->sid, ancestor->cid, rTree);
		//CNodeReplaceChild(ancestor->parent, ancestor, rNode);
		ancestor->type = R_INIT;
		child->type = R_SINK;
		child->recursion = ancestor;
		return;
	}
}

/*
 * Emit Related 
 */

static int numEntries = 0;
static int numEntriesLeaf = 0;
static int numCreated = 0;

static void emit(const char* file) {
	FILE* fp = fopen(file, "w");
	if(fp == NULL) {
		fprintf(stderr,"[kremlin] ERROR: couldn't open binary output file\n");
		exit(1);
	}
	emitRegion(fp, CPositionGetTree()->root->children[0], 0);
	fclose(fp);
	fprintf(stderr, "[kremlin] Created File %s : %d Regions Emitted (all %d leaves %d)\n", 
		file, numCreated, numEntries, numEntriesLeaf);

	//fp = fopen("kremlin_region_graph.dot","w");
	/*
	fprintf(fp,"digraph G {\n");
	emitDOT(fp,root);
	fprintf(fp,"}\n");
	fclose(fp);
	*/
}

static Bool isEmittable(Level level) {
	return level >= KConfigGetMinLevel() && level < KConfigGetMaxLevel();
	//return TRUE;
}

/**
 * emitNode - emit a node specific info
 *
 * - 64bit ID
 * - 64bit SID
 * - 64bit CID
 * - 64bit type
 * - 64bit recurse id
 * - 64bit # of instances
 * - 64bit DOALL flag
 * - 64bit child count (C)
 * - C * 64bit ID for children
 *
 * Total (40 + C * 8) bytes
 */

static void emitNode(FILE* fp, CNode* node) {
	numCreated++;
	MSG(DEBUG_CREGION, "Node id: %d, sid: %llx type: %d numInstance: %d nChildren: %d DOALL: %d\n", 
		node->id, node->sid, node->type, node->numInstance, node->children.size(), node->isDoall);
	fwrite(&node->id, sizeof(Int64), 1, fp);
	fwrite(&node->sid, sizeof(Int64), 1, fp);
	fwrite(&node->cid, sizeof(Int64), 1, fp);

	assert(node->type >=0 && node->type <= 2);
	UInt64 nodeType = node->type;
	fwrite(&nodeType, sizeof(Int64), 1, fp);
	
	UInt64 targetId = (node->recursion == NULL) ? 0 : node->recursion->id;
	fwrite(&targetId, sizeof(Int64), 1, fp);
	fwrite(&node->numInstance, sizeof(Int64), 1, fp);
	fwrite(&node->isDoall, sizeof(Int64), 1, fp);
	UInt64 numChildren = node->children.size();
	fwrite(&numChildren, sizeof(Int64), 1, fp);

	// TRICKY: not sure this is necessary but we go in reverse order to mimic
	// the behavior when we had a C linked-list for children
	for (int i = node->children.size()-1; i >= 0; --i) {
		CNode* child = node->children[i];
		assert(child != NULL);
		fwrite(&child->id, sizeof(Int64), 1, fp);    
	}
}

/**
 * emitStat - emit a level of stat
 *
 * - 64bit work
 * - 64bit tpWork (work after total-parallelism is applied)
 * - 64bit spWork (work after self-parallelism is applied)
 * - 2 * 64bit min / max SP
 * - 3 * 64bit total / min / max iteration count
 *
 * Total 64 bytes
 */


static void emitStat(FILE* fp, CStat* stat) {
	MSG(DEBUG_CREGION, "\tstat: sWork = %d, pWork = %d, nInstance = %d\n", 
		stat->totalWork, stat->spWork, stat->numInstance);
		
	fwrite(&stat->numInstance, sizeof(Int64), 1, fp);
	fwrite(&stat->totalWork, sizeof(Int64), 1, fp);
	fwrite(&stat->tpWork, sizeof(Int64), 1, fp);
	fwrite(&stat->spWork, sizeof(Int64), 1, fp);

	UInt64 minSPInt = (UInt64)(stat->minSP * 100.0);
	UInt64 maxSPInt = (UInt64)(stat->maxSP * 100.0);
	fwrite(&minSPInt, sizeof(Int64), 1, fp);
	fwrite(&maxSPInt, sizeof(Int64), 1, fp);

	fwrite(&stat->totalIterCount, sizeof(Int64), 1, fp);
	fwrite(&stat->minIterCount, sizeof(Int64), 1, fp);
	fwrite(&stat->maxIterCount, sizeof(Int64), 1, fp);

}

/**
 * emitRegion - emit a region tree
 *
 * for each region, here is the summarized binary format.
 *  - Node Info (emitNode)
 *  - N (64bit), which is # of stats
 *  - N * Stat Info (emitStat)
 * 
 */

static void emitRegion(FILE* fp, CNode* node, UInt level) {
    assert(fp != NULL);
    assert(node != NULL);

	UInt64 stat_size = CNodeGetStatSize(node);
	MSG(DEBUG_CREGION, "Emitting Node %d with %d stats\n", node->id, stat_size);
    assert(!node->stats.empty());
	
	if (isEmittable(level)) {
		numEntries++;
		if(node->children.empty())  
			numEntriesLeaf++; 

		emitNode(fp, node);

		fwrite(&stat_size, sizeof(Int64), 1, fp);
		// FIXME: run through stats in reverse?
		for (unsigned i = 0; i < stat_size; ++i) {
			CStat* s = node->stats[i];
			emitStat(fp, s);	
		}
	}

	// TRICKY: not sure this is necessary but we go in reverse order to mimic
	// the behavior when we had a C linked-list for children
	for (int i = node->children.size()-1; i >= 0; --i) {
		CNode* child = node->children[i];
		emitRegion(fp, child, level+1);
	}
}

void emitDOT(FILE* fp, CNode* node) {
	fprintf(stderr,"DOT: visiting %lu\n",node->id);

	// TRICKY: not sure this is necessary but we go in reverse order to mimic
	// the behavior when we had a C linked-list for children
	for (int i = node->children.size()-1; i >= 0; --i) {
		CNode* child = node->children[i];
		fprintf(fp, "\t%llx -> %llx;\n", node->id, child->id);
		emitDOT(fp, child);
	}
}



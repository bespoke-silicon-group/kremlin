
#include <stdlib.h>
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
static void   CNodeDelete(); 
static Bool   CNodeIsRNode(CNode* node); 
static CNode* CNodeFindChild(CNode* node, UInt64 sid, UInt64 callSite);
static void   CNodeAttach(CNode* node, CStat* region);
static void   CNodeLink(CNode* parent, CNode* child); 
static void   CNodeUpdate(CNode* node, RegionField* info);
static void   CNodeReplaceChild(CNode*, CNode*, CNode*);
static char*  CNodeToString(CNode* node); 
static Bool   CNodeIsSelfRNode(CTree* tree, CNode* node);
static CNodeType CNodeGetType(CNode* node);

static CStat* CNodeStatForward(CNode* current);
static CStat* CNodeStatBackward(CNode* current);

static CTree* CTreeCreate(CNode* root); 
static void   CTreeDelete(CTree* tree);
static CNode* CTreeFindAncestorBySid(CTree* tree, CNode* child);
static CTree* CTreeConvertFromSubTree(CNode* root, CNode* recurseNode); 
static void   CTreeEnterNode(CTree* tree, CNode* node);
static CNode* CTreeExitNode(CTree* tree);
static void   CTreeIncDepth(CTree* tree);
static void   CTreeDecDepth(CTree* tree);
static void   CTreeHandleRecursion(CTree* tree, CNode* child); 

static void emit(char* file);
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

void CRegionDeinit(char* file) {
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
	//assert(child->stat != NULL);

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
		//assert(current->stat != NULL);
		CNodeUpdate(current, info);
	}
#endif
	assert(info != NULL);
	CNodeUpdate(current, info);
	assert(current->stat != NULL);
	MSG(DEBUG_CREGION, "Update Node 0 - ID: %d Page: %d\n", current->id, current->stat->index);
	CNodeStatBackward(current);
	assert(current->parent != NULL);

	if (current->type == R_INIT) {
		CPositionSetNode(popped->parent);	

	} else {
		CPositionSetNode(current->parent);	
	}

	if (popped->type == R_SINK) {
		CNodeUpdate(popped, info);
		MSG(DEBUG_CREGION, "Update Node 1 - ID: %d Page: %d\n", popped->id, popped->stat->index);
		CNodeStatBackward(popped);
	} 
	printPosition();
	MSG(DEBUG_CREGION, "CRegionLeave: End \n"); 
}

/**
 * CRegionPush / CRegionPop
 *
 */

static CItem* stackTop;
static CItem* stackFreelist;

static CItem* CRegionStackAllocItem() {
	CItem* ret = NULL;
	if (stackFreelist == NULL) {
		ret = (CItem*)CRegionMemAlloc(sizeof(CItem), 0);
	} else {
		ret = stackFreelist;
		stackFreelist = ret->next;
	}
	return ret;
}

static void CRegionStackFreeItem(CItem* item) {
	CItem* old = stackFreelist;
	item->next = old;
	stackFreelist = item;
}

static void CRegionPush(CNode* node) {
	MSG(DEBUG_CREGION, "CRegionPush: ");
	CItem* prev = stackTop;
	//CItem* item = (CItem*)CRegionMemAlloc(sizeof(CItem), 0);
	CItem* item = CRegionStackAllocItem();
	item->prev = NULL;

	if (prev == NULL) {
		item->next = NULL;

	} else {
		//item = (CItem*)MemPoolAllocSmall(sizeof(CItem));
		item->next = prev;
		prev->prev = item;

	} 
	item->node = node;
	stackTop = item;
	MSG(DEBUG_CREGION, "%s\n", CNodeToString(node));
}

static CNode* CRegionPop() {
	MSG(DEBUG_CREGION, "CRegionPop: ");
	if (stackTop == NULL) {
		return NULL;
	}

	assert(stackTop != NULL);	
	CNode* ret = stackTop->node;
	MSG(DEBUG_CREGION, "%s\n", CNodeToString(ret));
	CItem* toDelete = stackTop;
	stackTop = toDelete->next;
	//CRegionMemFree(toDelete, sizeof(CItem), 0);
	CRegionStackFreeItem(toDelete);
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
	ret->index = index;
	ret->next = NULL;
	ret->prev = NULL;

	return ret;
}

static void CStatDelete(CStat* stat) {
	CRegionMemFree(stat, sizeof(CStat), 1); 
}


static void CStatUpdate(CStat* stat, RegionField* info) {
	assert(stat != NULL);
	assert(info != NULL);

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

static CStat* CNodeStatForward(CNode* node) {
	assert(node != NULL);
	CStat* current = node->stat;
	int index = 0;
	if (current != NULL)
		index = current->index + 1;

	MSG(DEBUG_CREGION, "CStatForward id %d to page %d\n", node->id, index);
	if (node->statStart == NULL) {
		CStat* ret = CStatCreate(0);
		node->stat = ret;
		node->statStart = ret;
		return ret;
	}

	if (current == NULL) {
		node->stat = node->statStart;
		return node->stat;
	}

	assert(current != NULL);
	if (current->next != NULL) {
		assert(current->next->index == current->index + 1);
		node->stat = current->next;
		return current->next;
	}
	
	CStat* ret = CStatCreate(current->index + 1);
	current->next = ret;
	ret->prev = current;
	node->stat = ret;
	return ret;
}

static CStat* CNodeStatBackward(CNode* node) {
	assert(node != NULL);
	CStat* current = node->stat;
	assert(current != NULL);
	MSG(DEBUG_CREGION, "CStatBackward id %d from page %d\n", node->id, current->index);
	//assert(current->prev->index == current->index - 1);
	//return current->prev;
	node->stat = current->prev;
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
	CStatUpdate(node->stat, info);
}

static CNode* CNodeFindChild(CNode* node, UInt64 sid, UInt64 callSite) {
	CNode* child = node->firstChild;
	//fprintf(stderr, "looking for sid : 0x%llx, callSite: 0x%llx\n", sid, callSite);
	while (child != NULL) {
		//fprintf(stderr, "\tcandidate sid : 0x%llx, callSite: 0x%llx\n", child->sid, child->cid);
		if (child->sid != sid)
			child = child->next;
		else if (child->rType != RegionFunc)
			return child;
		else if (child->cid == callSite) 
			return child;
		else
			child = child->next;
	}

	//fprintf(stderr, "\tnot found, creating one..\n");
	return NULL;
}

static void CNodeLink(CNode* parent, CNode* child) {
	assert(parent != NULL);
	CNode* prevFirstChild = (parent == NULL) ? NULL : parent->firstChild;
	child->next = prevFirstChild;
	parent->firstChild = child;
	parent->childrenSize++;
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
	ret->childrenSize = 0;
	ret->recursion = NULL;
	ret->firstChild = NULL;
	ret->numInstance = 0;
	ret->isDoall = 1;

	// debug info
	ret->code = 0xDEADBEEF;

	// stat
	ret->stat = NULL;
	ret->statStart = NULL;

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


static void CNodeDelete(CNode* node) { 
	CStatDelete(node->stat);
	MemPoolFreeSmall(node, sizeof(CNode)); 
}


// side effect: oldRegion's parent is set to NULL
static void CNodeReplaceChild(CNode* parent, CNode* oldRegion, CNode* newRegion) {
	CNode* child = parent->firstChild;
	oldRegion->parent = NULL;

	// case 1: first child
	if (child == oldRegion) {
		parent->firstChild = newRegion;
		newRegion->next = oldRegion->next;
		return;
	}

	// case 2: not the first child
	CNode* prev = child;
	child = child->next;
	while (child != NULL) {
		if (child == oldRegion) {
			prev->next = newRegion;
			newRegion->next = oldRegion->next;
			return;
		}
		prev = child;
		child = child->next;
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
	UInt64 childId = (node->firstChild == NULL) ? 0 : node->firstChild->id;
	sprintf(_buf, "id: %d, type: %5s, parent: %d, firstChild: %d, sid: %llx", 
		node->id, _strType[node->type], parentId, childId, node->sid);
	return _buf;
}

static int CNodeGetStatSize(CNode* node) {
	if (node->statStart == NULL)
		return 0;
	
	int num = 0;
	CStat* current = node->statStart;
	while (current != NULL) {
		num++;
		current = current->next;
	}
	return num;
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
	ret->stackTop = NULL;
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

static void emit(char* file) {
	FILE* fp = fopen(file, "w");
	emitRegion(fp, CPositionGetTree()->root->firstChild, 0);
	fclose(fp);
	fprintf(stderr, "[kremlin] Created %d Emitted (all %d leaves %d)\n", 
		numCreated, numEntries, numEntriesLeaf);

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
		node->id, node->sid, node->type, node->numInstance, node->childrenSize, node->isDoall);
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
	fwrite(&node->childrenSize, sizeof(Int64), 1, fp);

	CNode* current = node->firstChild;
	int i;
	UInt64 size = node->childrenSize;
	for (i=0; i<size; i++) {
		assert(current != NULL);
		fwrite(&current->id, sizeof(Int64), 1, fp);    
		current = current->next;
	}           
	assert(current == NULL);
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
	MSG(DEBUG_CREGION, "\t[%d] stat: sWork = %d, pWork = %d, nInstance = %d\n", 
		stat->index, stat->totalWork, stat->spWork, stat->numInstance);
		
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
	CStat* stat = node->statStart;
	UInt64 statSize = CNodeGetStatSize(node);
	//fprintf(stderr, "emitting region %llu at level %u\n", node->region->id,level);
	MSG(DEGBUG_CREGION, "Emitting Node %d with %d stats\n", node->id, statSize);
    assert(fp != NULL);
    assert(node != NULL);
    assert(stat != NULL);
	//assert(region->numInstance > 0);
	
	if (isEmittable(level)) {
		numEntries++;
		if(node->childrenSize == 0)  
			numEntriesLeaf++; 

		emitNode(fp, node);

		fwrite(&statSize, sizeof(Int64), 1, fp);
		while (stat != NULL) {
			emitStat(fp, stat);	
			stat = stat->next;
		}
	}

#if 0
		fwrite(&node->id, sizeof(Int64), 1, fp);
		fwrite(&node->sid, sizeof(Int64), 1, fp);
		fwrite(&node->cid, sizeof(Int64), 1, fp);
		fwrite(&node->numInstance, sizeof(Int64), 1, fp);

		fwrite(&stat->totalWork, sizeof(Int64), 1, fp);
		fwrite(&stat->tpWork, sizeof(Int64), 1, fp);
		fwrite(&stat->spWork, sizeof(Int64), 1, fp);

		UInt64 minSPInt = (UInt64)(stat->minSP * 100.0);
		UInt64 maxSPInt = (UInt64)(stat->maxSP * 100.0);
		fwrite(&minSPInt, sizeof(Int64), 1, fp);
		fwrite(&maxSPInt, sizeof(Int64), 1, fp);
		fwrite(&stat->isDoall, sizeof(Int64), 1, fp);

		fwrite(&node->totalChildCount, sizeof(Int64), 1, fp);
		fwrite(&node->minChildCount, sizeof(Int64), 1, fp);
		fwrite(&node->maxChildCount, sizeof(Int64), 1, fp);
		fwrite(&node->childrenSize, sizeof(Int64), 1, fp);
		CNode* current = node->firstChild;
		int i;
		for (i=0; i<size; i++) {
			assert(current != NULL);
			fwrite(&current->id, sizeof(Int64), 1, fp);    
			current = current->next;
		}    
#endif


	CNode* current = node->firstChild;
	int i;
	for (i=0; i<node->childrenSize; i++) {
		emitRegion(fp, current, level+1);
        current = current->next;
	}
	assert(current == NULL);
}

void emitDOT(FILE* fp, CNode* node) {
	fprintf(stderr,"DOT: visiting %lu\n",node->id);
	CNode* child_node = node->firstChild;
	UInt64 size = node->childrenSize;
	int i;
	for(i = 0; i < size; ++i) {
		fprintf(fp,"\t%llx -> %llx;\n", node->id, child_node->id);
		emitDOT(fp,child_node);
		child_node = child_node->next;
	}
}



#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "kremlin.h"
#include "MemMapAllocator.h"
#include "CRegion.h"

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


static CStat* CStatCreate(int index);
static void   CStatDelete(CStat* region);
static CStat* CStatForward(CStat* current);
static CStat* CStatBackward(CStat* current);

static CNode* CNodeCreate(SID sid, CID cid); 
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

/******************************** 
 * Public Functions
 *********************************/

void CRegionInit() {
	CPositionInit();
	//fprintf(stderr, "cregionInit..");
	//CRegion* region = createCRegion(0, 0);
	//root = createCNode(NULL, region);

	//current = root;
	//fprintf(stderr, "done!..\n");
}

void CRegionDeinit(char* file) {
	emit(file);
}

void printPosition() {
	MSG(0, "Curr %s Node: %s\n", CPositionToStr(), CNodeToString(CPositionGetNode()));
}

void CRegionEnter(SID sid, CID cid) {

	CTree* tree = CPositionGetTree();
	CNode* parent = CPositionGetNode();
	CNode* child = NULL;

	// corner case: no graph exists 
	if (tree == NULL) {
		child = CNodeCreate(sid, cid);
		CPositionSet(CTreeCreate(child), child);
		MSG(0, "CRegionEnter: sid: root -> 0x%llx, callSite: 0x%llx\n", 
			sid, cid);
		return;
	}
	
	assert(parent != NULL);
	MSG(0, "CRegionEnter: sid: 0x%llx -> 0x%llx, callSite: 0x%llx\n", 
		parent->sid, sid, cid);

	// make sure a child node exist
	child = CNodeFindChild(parent, sid, cid);
	if (child == NULL) {
		// case 3) step a - new node required
		child = CNodeCreate(sid, cid);
		CNodeLink(parent, child);
		CTreeHandleRecursion(tree, child);
	} 

	// set position, push the current region to the current tree
	switch (child->type) {
	case SELF_R:
		CPositionSetNode(tree->root);
		CTreeEnterNode(tree, child);
		break;
	case EXT_R:
		CPositionSet(child->tree, child->tree->root);
		CTreeEnterNode(tree, child);
		break;
	case NORMAL:
		CPositionSetNode(child);
		CTreeEnterNode(tree, child);
		break;
	}
	CStatForward(child->stat);
	printPosition();

	MSG(0, "CRegionEnter: End\n"); 
}

// at the end of a region execution,
// pass the region exec info.
// update the passed info and 
// set current pointer to one level higher
void CRegionLeave(RegionField* info) {
	MSG(0, "CRegionLeave: \n"); 
	// don't update if we didn't give it any info
	// this happens when we are out of range for logging
	CTree* tree = CPositionGetTree();
	CNode* current = CPositionGetNode();
	assert(current != NULL);

	if (info != NULL) {
		assert(current != NULL);
		assert(current->stat != NULL);
		//CNodeUpdate(current, info);
	}
	CTreeDecDepth(tree);
	CStatBackward(current);
	CPositionSetNode(current->parent);	
	printPosition();
}



/******************************** 
 * CPosition Management 
 *********************************/
static CPosition _curPosition;

static void CPositionInit() {
	_curPosition.tree = NULL;
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

static void CPositionSet(CTree* tree, CNode* node) {
	_curPosition.tree = tree;
	_curPosition.node = node;

}


static void CPositionSetNode(CNode* node) {
	_curPosition.node = node;

}

static void CPositionSetTree(CTree* tree) {
	_curPosition.tree = tree;
}


static char _bufCur[16];
static char* CPositionToStr() {
	CTree* tree = CPositionGetTree();
	CNode* node = CPositionGetNode();
	UInt64 treeId = (tree == NULL) ? 0 : tree->id;
	UInt64 nodeId = (node == NULL) ? 0 : node->id;
	sprintf(_bufCur, "<%2d:%5d>", treeId, nodeId);
	return _bufCur;
}



/*****************************
 * CStat Related Routines
 *****************************/

static CStat* CStatCreate(int index) {
	CStat* ret = (CStat*)MemPoolAllocSmall(sizeof(CStat));
	ret->totalWork = 0;
	ret->tpWork = 0;
	ret->spWork = 0;
	ret->minSP = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxSP = 0;
	ret->readCnt = 0;
	ret->writeCnt = 0;
	ret->loadCnt = 0;
	ret->storeCnt = 0;
	ret->isDoall = 1;

	ret->index = index;
	ret->next = NULL;
	ret->prev = NULL;
	return ret;
}

static void CStatDelete(CStat* stat) {
	MemPoolFreeSmall(stat, sizeof(CStat)); 
}


static void CStatUpdate(CStat* stat, RegionField* info) {
	assert(stat != NULL);
	assert(info != NULL);

	MSG(3, "CStatUpdate: work = %d, spWork = %d\n", info->work, info->spWork);
	
	double sp = (double)info->work / (double)info->spWork;
	if (stat->minSP > sp) stat->minSP = sp;
	if (stat->maxSP < sp) stat->maxSP = sp;
	stat->totalWork += info->work;
	stat->tpWork += info->cp;
	stat->spWork += info->spWork;

		// handle P bit for DOALL identification
	if (info->isDoall == 0)
		stat->isDoall = 0;

#ifdef EXTRA_STATS
	stat->readCnt += info->readCnt;
	stat->writeCnt += info->writeCnt;
	stat->loadCnt += info->loadCnt;
	stat->storeCnt += info->storeCnt;
#endif
}

static CStat* CStatForward(CStat* current) {
	if (current == NULL)
		return CStatCreate(0);

	if (current->next != NULL) {
		assert(current->next->index == current->index + 1);
		return current->next;
	}
	
	CStat* ret = CStatCreate(current->index + 1);
	current->next = ret;
	return ret;
}

static CStat* CStatBackward(CStat* current) {
	assert(current->prev != NULL);
	assert(current->prev->index == current->index - 1);
	return current->prev;
}

/****************************
 * CNode Related Routines 
 ***************************/

static void CNodeUpdate(CNode* node, RegionField* info) {
	assert(info != NULL);
	MSG(3, "CRegionUpdate: cid(0x%lx), work(0x%lx), cp(%lx), spWork(%lx)\n", 
			info->callSite, info->work, info->cp, info->spWork);
	MSG(3, "current region: id(0x%lx), sid(0x%lx), cid(0x%lx)\n", 
			node->id, node->sid, node->cid);

	assert(node->cid == info->callSite);
	assert(node->numInstance >= 0);

	node->numInstance++;
	node->totalChildCount += info->childCnt;
	if (node->minChildCount > info->childCnt) 
		node->minChildCount = info->childCnt;
	if (node->maxChildCount < info->childCnt) 
		node->maxChildCount = info->childCnt;

	CStatUpdate(node->stat, info);
}

static CNode* CNodeFindChild(CNode* node, UInt64 sid, UInt64 callSite) {
	CNode* child = node->firstChild;
	//fprintf(stderr, "looking for sid : 0x%llx, callSite: 0x%llx\n", sid, callSite);
	while (child != NULL) {
		//fprintf(stderr, "\tcandidate sid : 0x%llx, callSite: 0x%llx\n", region->sid, region->callSite);
		if (child->sid == sid && child->cid == callSite) {
			return child;
		} else {
			child = child->next;
		}
	}
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


static CNode* CNodeCreate(SID sid, CID cid) {
	CNode* ret = (CNode*)MemPoolAllocSmall(sizeof(CNode));
	MSG(3, "CNode: created CNode at 0x%x\n", ret);

	// basic info
	ret->type = NORMAL;
	ret->id = CNodeAllocId();
	ret->sid = sid;
	ret->cid = cid;
	ret->childrenSize = 0;
	ret->firstChild = NULL;
	ret->numInstance = 0;

	// child info	
	ret->totalChildCount = 0;
	ret->minChildCount = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxChildCount = 0;

	// debug info
	ret->code = 0xDEADBEEF;

	// stat
	ret->stat = NULL;

	return ret;
}

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
static CNode* CNodeConvertToRecursive(CNode* base) {
	CNode* ret = CNodeCreate(base->sid, base->cid);
	return ret;
}

static char _buf[256];
static char* CNodeToString(CNode* node) {
	if (node == NULL) {
		sprintf(_buf, "NULL"); 
		return _buf;
	}

	UInt64 parentId = (node->parent == NULL) ? 0 : node->parent->id;
	UInt64 childId = (node->firstChild == NULL) ? 0 : node->firstChild->id;
	sprintf(_buf, "id: %d, parent: %d, firstChild: %d, sid: %llx", 
		node->id, parentId, childId, node->sid);
	return _buf;
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
	CTree* ret = (CTree*)MemPoolAllocSmall(sizeof(CTree));
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
	MSG(0, "CTreeFindAncestor: sid: 0x%llx....", child->sid);
	SID sid = child->sid;
	CNode* node = child->parent;
	
	while (node != NULL) {
		if (node->sid == sid) {
			MSG(0, "found\n");
			return node;
		}

		node = node->parent;
	}
	MSG(0, "not found\n");
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

/**
 * CTreeEnterNode / CTreeExitNode
 *
 * CTree manages a stack for active regions. 
 * CTreeEnterNode / CTreeExitNode push/pop CNode at the stack.
 */

static void CTreeEnterNode(CTree* tree, CNode* node) {
	CItem* prev = tree->stackTop;
	CItem* item = (CItem*)MemPoolAllocSmall(sizeof(CItem));
	item->prev = NULL;

	if (prev == NULL) {
		item->next = NULL;

	} else {
		item = (CItem*)MemPoolAllocSmall(sizeof(CItem));
		item->next = prev;
		prev->prev = item;

	} 
	item->node = node;
	tree->stackTop = item;
}

static CNode* CTreeExitNode(CTree* tree) {
	assert(tree->stackTop != NULL);	
	CNode* ret = tree->stackTop->node;
	CItem* toDelete = tree->stackTop;
	tree->stackTop = toDelete->next;
	MemPoolFreeSmall(toDelete, sizeof(CItem));
	
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

	} else if (ancestor == tree->root) {
		CNodeConvertToSelfRNode(child, tree);
		return;

	} else {
		assert(ancestor->parent != NULL);
		CTree* rTree = CTreeConvertFromSubTree(ancestor, child);	
		CNode* rNode = CNodeCreateExtRNode(ancestor->sid, ancestor->cid, rTree);
		CNodeReplaceChild(ancestor->parent, ancestor, rNode);
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
	emitRegion(fp, CPositionGetTree()->root, 0);
	fclose(fp);
	fprintf(stderr, "[kremlin] Created %d Emitted (all %d leaves %d)\n", 
		numCreated, numEntries, numEntriesLeaf);

	fp = fopen("kremlin_region_graph.dot","w");
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


static void emitRegion(FILE* fp, CNode* node, UInt level) {
	CStat* stat = node->stat;
	//fprintf(stderr, "emitting region %llu at level %u\n", node->region->id,level);
    assert(fp != NULL);
    assert(node != NULL);
    assert(stat != NULL);
	//assert(region->numInstance > 0);

	UInt64 size = node->childrenSize;

	if (isEmittable(level))
	{
		numEntries++;
		if(size == 0) { numEntriesLeaf++; }

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
		assert(current == NULL);
	}

	CNode* current = node->firstChild;

	int i;
	for (i=0; i<size; i++) {
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



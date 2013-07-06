#include "CTree.h"
#include "CNode.h"
#include "MemMapAllocator.h"
#include "debug.h"

static UInt64 lastTreeId = 0; // TODO: make this member var?

UInt64 CTree::allocId() { return ++lastTreeId; }

CTree* CTree::create(CNode* root) {
	CTree* ret = (CTree*)MemPoolAllocSmall(sizeof(CTree));
	ret->id = allocId();
	ret->maxDepth = 0;
	ret->currentDepth = 0;
	ret->parent = NULL;
	ret->root = root;
	return ret;
}

void CTree::destroy(CTree* tree) {
	// first, traverse and free every node in the tree
	// TODO Here

	// second, free the tree storage
	MemPoolFreeSmall(tree, sizeof(CTree));
}

/**
 * Create a CTree from a subtree
 */
CTree* CTree::createFromSubTree(CNode* root, CNode* recurseNode) {
	CTree* ret = CTree::create(root);
	recurseNode->tree = ret;
	return ret;
}

// XXX: "this" unused???
CNode* CTree::findAncestorBySid(CNode* child) {
	MSG(DEBUG_CREGION, "findAncestor: sid: 0x%llx....", child->sid);
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


void CTree::handleRecursion(CNode* child) {
	// detect a recursion with a new node
	// - find an ancestor where ancestor.sid == child.sid
	// - case a) no ancestor found - no recursion
	// - case b) recursion to the root node: self recursion
	//			 transform child to RNode
	// - case c) recursion to a non-root node: a new tree needed
	//	       - create a CTree from a subtree starting from the ancestor
	//         - set current tree and node appropriately

	CNode* ancestor = findAncestorBySid(child);

	if (ancestor == NULL) {
		return;
#if 0
	} else if (ancestor == this->root) {
		child->convertToSelfRNode(this);
		return;
#endif

	} else {
		assert(ancestor->parent != NULL);
		//CTree* rTree = CTree::createFromSubTree(ancestor, child);	
		//CNode* rNode = CNode::createExtRNode(ancestor->sid, ancestor->cid, rTree);
		//ancestor->parent->replaceChild(ancestor, rNode);
		ancestor->type = R_INIT;
		child->type = R_SINK;
		child->recursion = ancestor;
		return;
	}
}

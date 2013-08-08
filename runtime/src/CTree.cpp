#include "CTree.h"
#include "CNode.h"
#include "MemMapAllocator.h"
#include "debug.h"

static UInt64 last_id = 0; // TODO: make this member var?

UInt64 CTree::getNewID() { return ++last_id; }

CTree* CTree::create(CNode* root) {
	CTree* ret = (CTree*)MemPoolAllocSmall(sizeof(CTree));
	ret->id = getNewID();
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

// TODO: this appears as though it should be a member of CNode function
// (doesn't use "this" pointer anywhere)
CNode* CTree::findAncestorBySid(CNode* node) {
	MSG(DEBUG_CREGION, "findAncestor: sid: 0x%llx....", node->sid);
	SID sid = node->sid;
	CNode* ancestor = node->parent;
	
	while (ancestor != NULL) {
		if (ancestor->sid == sid) {
			return ancestor;
		}

		ancestor = ancestor->parent;
	}
	return NULL;
}


void CTree::handleRecursion(CNode* node) {
	// detect a recursion with a new node
	// - find an ancestor where ancestor.sid == node.sid
	// - case a) no ancestor found - no recursion
	// - case b) recursion to the root node: self recursion
	//			 transform node to RNode
	// - case c) recursion to a non-root node: a new tree needed
	//	       - create a CTree from a subtree starting from the ancestor
	//         - set current tree and node appropriately

	CNode* ancestor = findAncestorBySid(node);

	if (ancestor == NULL) {
		return;
#if 0
	} else if (ancestor == this->root) {
		node->convertToSelfRNode(this);
		return;
#endif

	} else {
		assert(ancestor->parent != NULL);
		//CTree* rTree = CTree::createFromSubTree(ancestor, node);	
		//CNode* rNode = CNode::createExtRNode(ancestor->sid, ancestor->cid, rTree);
		//ancestor->parent->replaceChild(ancestor, rNode);
		ancestor->type = R_INIT;
		node->type = R_SINK;
		node->recursion = ancestor;
		return;
	}
}

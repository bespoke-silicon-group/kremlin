#ifndef _CTREE_H_
#define _CTREE_H_

#include "ktypes.h"

class CNode;

class CTree {
public:
	UInt64 id;
	int maxDepth;
	int currentDepth;
	CNode* root;
	CNode* parent;

	CNode* findAncestorBySid(CNode* child);
	void   handleRecursion(CNode* child); 

	// XXX the following functions are unimplemented and unused
	void   enterNode(CNode* node);
	CNode* exitNode();

	static CTree* create(CNode* root); 
	static void   destroy(CTree* tree);
	static CTree* createFromSubTree(CNode* root, CNode* recurseNode); 
	static UInt64 allocId();

	/* Getters */

	bool isRoot() { return this->parent == NULL; }
	CNode* getParent() { return this->parent; }
	CNode* getRoot() { return this->root; }
	UInt getMaxDepth() { return this->maxDepth; }
	UInt getDepth() { return this->currentDepth; }

	/* Setters */

	void incDepth() {
		this->currentDepth++;
		if (this->currentDepth > this->maxDepth)
			this->maxDepth = this->currentDepth;
	}

	void decDepth() {
		this->currentDepth--;
	}
}; 


#endif

#ifndef _CTREE_H_
#define _CTREE_H_

#include "ktypes.h"

class CNode;

class CTree {
private:
	UInt64 id;
	CNode* root;

	/*! Returns a new (unique) ID */
	static UInt64 getNewID();
	
public:
	CNode* getRoot() { return root; }

	/*!
	 * Returns an ancestor with the same static region ID. If no such ancestor
	 * is found, returns NULL.
	 *
	 * @param node The node whose ancestor we wish to find.
	 * @return The ancestor with the same static ID; NULL if none exist.
	 */
	CNode* findAncestorBySid(CNode* node);

	/*!
	 * Checks if a node is a recursive instance of an already existing node.
	 * If the node is a recursive node, it sets the type of the node and its
	 * ancestor (of which it is a recursive instance) and creates a link from
	 * the node to this ancestor.
	 *
	 * @param node Node for which to handle recursion.
	 */
	void handleRecursion(CNode* node); 

	static CTree* create(CNode* root); 
	static void destroy(CTree* tree);
	static CTree* createFromSubTree(CNode* root, CNode* recurseNode); 
}; 


#endif

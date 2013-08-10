#include <stdlib.h>
#include <stack>
#include <utility> // for std::pair
#include <sstream>

#include "config.h"
#include "kremlin.h"
#include "MemMapAllocator.h"

#include "CRegion.h"
#include "CNode.h"
#include "CStat.h"

static std::stack<CNode*> c_region_stack;

static void pushOnRegionStack(CNode* node);
static CNode* popFromRegionStack();

static void emit(const char* file);
static void emitRegion(FILE* fp, CNode* node, UInt level);

/******************************** 
 * CPosition Management 
 *********************************/
static CNode* region_tree_root; // TODO: make member var of profiler?
static CNode* curr_region_node; // TODO: make mem var of profiler?

static const char* currPosStr() {
	CNode* node = curr_region_node;
	UInt64 nodeId = (node == NULL) ? 0 : node->id;
	std::stringstream ss;
	ss << "<" << nodeId << ">";
	return ss.str().c_str();
}

static void printPosition() {
	MSG(DEBUG_CREGION, "Curr %s Node: %s\n", currPosStr(), 
			curr_region_node->toString());
}


/******************************** 
 * Public Functions
 *********************************/

void CRegionInit() {
	// create dummy root node
	region_tree_root = new CNode(0, 0, RegionFunc); // XXX: mem leak
	curr_region_node = region_tree_root;
	assert(root != NULL);
}

void CRegionDeinit(const char* file) {
	assert(popFromRegionStack() == NULL);
	emit(file);
}

void CRegionEnter(SID sid, CID cid, RegionType type) {
	if (KConfigGetCRegionSupport() == FALSE)
		return;

	CNode* parent = curr_region_node;
	CNode* child = NULL;

	// corner case: no graph exists 
#if 0
	if (parent == NULL) {
		child = new CNode(sid, cid); // XXX: wrong
		curr_region_node = child;
		pushOnRegionStack(child);
		MSG(0, "CRegionEnter: sid: root -> 0x%llx, callSite: 0x%llx\n", 
			sid, cid);
		return;
	}
#endif
	
	assert(parent != NULL);
	MSG(DEBUG_CREGION, "CRegionEnter: sid: 0x%llx -> 0x%llx, callSite: 0x%llx\n", 
		parent->sid, sid, cid);

	// make sure a child node exist
	child = parent->findChild(sid, cid);
	if (child == NULL) {
		// case 3) step a - new node required
		child = new CNode(sid, cid, type); // XXX: mem leak
		parent->linkChild(child);
		if (KConfigGetRSummarySupport())
			child->handleRecursion();
	} 

	child->statForward();

	assert(child != NULL);
	// set position, push the current region to the current tree
	switch (child->type) {
	case R_INIT:
		curr_region_node = child;
		break;
	case R_SINK:
		assert(child->recursion != NULL);
		curr_region_node = child->recursion;
		child->recursion->statForward();
		break;
	case NORMAL:
		curr_region_node = child;
		break;
	}
	pushOnRegionStack(child);
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
	CNode* current = curr_region_node;
	CNode* popped = popFromRegionStack();
	assert(popped != NULL);
	assert(current != NULL);

	MSG(DEBUG_CREGION, "Curr %s Node: %s\n", currPosStr(), current->toString());
#if 0
	if (info != NULL) {
		assert(current != NULL);
		current->update(info);
	}
#endif

	assert(info != NULL);
	current->update(info);

	assert(current->curr_stat_index != -1);
	MSG(DEBUG_CREGION, "Update Node 0 - ID: %d Page: %d\n", current->id, 
		current->curr_stat_index);
	current->statBackward();
	assert(current->parent != NULL);

	if (current->type == R_INIT) {
		curr_region_node = popped->parent;

	} else {
		curr_region_node = current->parent;
	}

	if (popped->type == R_SINK) {
		popped->update(info);
		MSG(DEBUG_CREGION, "Update Node 1 - ID: %d Page: %d\n", popped->id, 
			popped->curr_stat_index);
		popped->statBackward();
	} 
	printPosition();
	MSG(DEBUG_CREGION, "CRegionLeave: End \n"); 
}

/**
 * pushOnRegionStack / popFromRegionStack
 *
 */

void pushOnRegionStack(CNode* node) {
	MSG(DEBUG_CREGION, "addOnRegionStack: ");

	c_region_stack.push(node);

	MSG(DEBUG_CREGION, "%s\n", node->toString());
}

CNode* popFromRegionStack() {
	MSG(DEBUG_CREGION, "popFromRegionStack: ");

	if (c_region_stack.empty()) {
		return NULL;
	}

	CNode* ret = c_region_stack.top();
	c_region_stack.pop();
	MSG(DEBUG_CREGION, "%s\n", ret->toString());

	return ret;
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
	emitRegion(fp, region_tree_root->children[0], 0);
	fclose(fp);
	fprintf(stderr, "[kremlin] Created File %s : %d Regions Emitted (all %d leaves %d)\n", 
		file, numCreated, numEntries, numEntriesLeaf);

	// TODO: make DOT printing a command line option
#if 0
	fp = fopen("kremlin_region_graph.dot","w");
	fprintf(fp,"digraph G {\n");
	emitDOT(fp,root);
	fprintf(fp,"}\n");
	fclose(fp);
#endif
}

static bool isEmittable(Level level) {
	return level >= KConfigGetMinLevel() && level < KConfigGetMaxLevel();
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

	UInt64 stat_size = node->getStatSize();
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
	fprintf(stderr,"DOT: visiting %llu\n",node->id);

	// TRICKY: not sure this is necessary but we go in reverse order to mimic
	// the behavior when we had a C linked-list for children
	for (int i = node->children.size()-1; i >= 0; --i) {
		CNode* child = node->children[i];
		fprintf(fp, "\t%llx -> %llx;\n", node->id, child->id);
		emitDOT(fp, child);
	}
}



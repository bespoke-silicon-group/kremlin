#include <stdlib.h>
#include <stack>
#include <utility> // for std::pair
#include <sstream>

#include "config.h"
#include "kremlin.h"
#include "MemMapAllocator.h"

#include "CRegion.h"
#include "ProfileNode.hpp"
#include "ProfileNodeStats.hpp"

static std::stack<ProfileNode*> c_region_stack;

static void pushOnRegionStack(ProfileNode* node);
static ProfileNode* popFromRegionStack();

static void writeProgramStats(const char* filename);
static void writeRegionStats(FILE* fp, ProfileNode* node, UInt level);

/******************************** 
 * CPosition Management 
 *********************************/
static ProfileNode* region_tree_root; // TODO: make member var of profiler?
static ProfileNode* curr_region_node; // TODO: make mem var of profiler?

/*!
 * Returns a string representing the ID of the curent region node.
 *
 * @return String of the form "<ID>" where ID is the ID of the region that is
 * currently active (or 0 if no region is active).
 */
static const char* getCurrentRegionIDString() {
	ProfileNode* node = curr_region_node;
	UInt64 nodeId = (node == NULL) ? 0 : node->id;
	std::stringstream ss;
	ss << "<" << nodeId << ">";
	return ss.str().c_str();
}

/*!
 * Prints the node that is currently active.
 */
static void printCurrRegionNode() {
	MSG(DEBUG_CREGION, "Curr %s Node: %s\n", getCurrentRegionIDString(), 
			curr_region_node->toString());
}


/******************************** 
 * Public Functions
 *********************************/

void initRegionTree() {
	assert(region_tree_root == NULL);
	region_tree_root = new ProfileNode(0, 0, RegionFunc);
	curr_region_node = region_tree_root;
	assert(region_tree_root != NULL);
	assert(region_tree_root == curr_region_node);
}

void printProfiledData(const char* filename) {
	assert(filename != NULL);
	assert(region_tree_root != NULL);
	assert(!c_region_stack.empty());
	writeProgramStats(filename);
}

void deinitRegionTree() {
	assert(region_tree_root != NULL);
	assert(curr_region_node == region_tree_root);
	delete region_tree_root;
	curr_region_node = region_tree_root = NULL;
}

void openRegionContext(SID region_static_id, CID region_callsite_id, 
						RegionType region_type) {
	assert(region_tree_root != NULL);
	assert(curr_region_node != NULL);
	unsigned prev_stack_size = c_region_stack.size();

	ProfileNode* parent = curr_region_node;

	MSG(DEBUG_CREGION, "openRegionContext: static_id: 0x%llx -> 0x%llx, callSite: 0x%llx\n", 
		parent->static_id, region_static_id, region_callsite_id);

	ProfileNode* child = parent->getChild(region_static_id, region_callsite_id);

	// If no child was found with this static and callsite ID, we'll create a
	// new ProfileNode for this child.
	if (child == NULL) {
		// TODO: make body of this if statement a separate function
		child = new ProfileNode(region_static_id, region_callsite_id, region_type); // XXX: mem leak
		parent->addChild(child);
		if (kremlin_config.summarizeRecursiveRegions())
			child->handleRecursion();
	} 

	child->moveToNextStats();

	// set position, push the current region to the current tree
	switch (child->node_type) {
		case R_INIT:
			curr_region_node = child;
			break;
		case R_SINK:
			assert(child->recursion != NULL);
			curr_region_node = child->recursion;
			child->recursion->moveToNextStats();
			break;
		case NORMAL:
			curr_region_node = child;
			break;
	}
	pushOnRegionStack(child);
	printCurrRegionNode();

	MSG(DEBUG_CREGION, "openRegionContext: End\n"); 
	assert(!c_region_stack.empty());
	assert(child->curr_stat_index >= 0);
	assert(c_region_stack.size() == prev_stack_size+1);
	assert(child->node_type != R_SINK);
}

void closeRegionContext(RegionStats *region_stats) {
	assert(region_stats != NULL);
	assert(!c_region_stack.empty());
	assert(region_tree_root != NULL);
	assert(curr_region_node != NULL);
	assert(curr_region_node->curr_stat_index >= 0);
	assert(curr_region_node != region_tree_root);
	assert(curr_region_node->parent != NULL); // redundant with curr != root?
	unsigned prev_stack_size = c_region_stack.size();

	MSG(DEBUG_CREGION, "closeRegionContext: Begin\n"); 
	MSG(DEBUG_CREGION, "Curr %s Node: %s\n", getCurrentRegionIDString(), curr_region_node->toString());

#if 0
	// don't update stats if we didn't give it any region_stats
	// this happens when we are out of range for logging
	if (region_stats != NULL) {
		assert(curr_region_node != NULL);
		curr_region_node->update(region_stats);
	}
#endif

	curr_region_node->addStats(region_stats);

	MSG(DEBUG_CREGION, "Updating Current Node - ID: %llu, Stat Index: %d\n", curr_region_node->id, 
		curr_region_node->curr_stat_index);
	curr_region_node->moveToPrevStats();

	// By construction, curr_region_node will never be an R_SINK: it will
	// always move to the associated R_INIT. Also, a node will only be an
	// R_INIT if it has a corresponding R_SINK. Therefore, if the current node
	// is an R_INIT, then we don't want to go to the parent node of the
	// current node, we want the parent node of the region that is at the top
	// of the region stack (i.e. the parent of the R_SINK node).
	ProfileNode* exited_region = popFromRegionStack();
	if (curr_region_node->node_type == R_INIT) {
		curr_region_node = exited_region->parent;
	}
	else {
		curr_region_node = curr_region_node->parent;
	}

	if (exited_region->node_type == R_SINK) {
		exited_region->addStats(region_stats);
		MSG(DEBUG_CREGION, "Updating R_SINK Node - ID: %llu, Stat Index: %d\n", exited_region->id, 
			exited_region->curr_stat_index);
		exited_region->moveToPrevStats();
	} 
	printCurrRegionNode();
	MSG(DEBUG_CREGION, "closeRegionContext: End \n"); 
	assert(c_region_stack.size() == prev_stack_size-1);
}



/*!
 * Pushes a node onto the region stack.
 *
 * @param node The node to add to the region stack.
 * @pre The specified node is non-NULL
 * @post The region stack will not be empty.
 */
void pushOnRegionStack(ProfileNode* node) {
	assert(node != NULL);
	MSG(DEBUG_CREGION, "pushOnRegionStack: ");
	MSG(DEBUG_CREGION, "%s\n", node->toString());

	c_region_stack.push(node);
	assert(!c_region_stack.empty());
}

/*!
 * Pops a node from the region stack.
 *
 * @return The node that was at the top of the region stack.
 * @pre The region stack is not empty.
 * @post The returned node will be non-NULL.
 */
ProfileNode* popFromRegionStack() {
	assert(!c_region_stack.empty());
	MSG(DEBUG_CREGION, "popFromRegionStack: ");

	ProfileNode* ret = c_region_stack.top();
	c_region_stack.pop();
	MSG(DEBUG_CREGION, "%s\n", ret->toString());

	assert(ret != NULL);
	return ret;
}




/*
 * Emit Related 
 */

static int numEntries = 0;
static int numEntriesLeaf = 0;
static int numCreated = 0;

/*!
 * Writes statistics for all nodes in the region tree to a specified file.
 *
 * @param filename The location we will write the stats too.
 * @pre filename is non-NULL
 * @pre The region tree has been initialized (i.e. is non-NULL)
 * @pre There is exactly one child of the root region (i.e. main)
 */
static void writeProgramStats(const char* filename) {
	assert(filename != NULL);
	assert(region_tree_root != NULL);
	assert(region_tree_root->getNumChildren() == 1);

	FILE* fp = fopen(filename, "w");
	if(fp == NULL) {
		fprintf(stderr,"[kremlin] ERROR: couldn't open binary output file\n");
		// TODO: throw exception rather than dying... so we can possibly reask
		// for the correct filename
		exit(1);
	}
	writeRegionStats(fp, region_tree_root->children[0], 0);
	fclose(fp);
	fprintf(stderr, "[kremlin] Created File %s : %d Regions Emitted (all %d leaves %d)\n", 
		filename, numCreated, numEntries, numEntriesLeaf);

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
	return level >= kremlin_config.getMinProfiledLevel() && level < kremlin_config.getMaxProfiledLevel();
}

/*!
 * Writes profiling stats associated with a given node to a file.
 *
 * @remark The output for the node will contain 8C + 40 bytes, in the
 * following format:
 *
 * 1. 64bit ID
 * 2. 64bit SID
 * 3. 64bit CID
 * 4. 64bit node_type
 * 5. 64bit recurse id
 * 6. 64bit # of instances
 * 7. 64bit DOALL flag
 * 8. 64bit child count (C)
 * 9. C * 64bit ID for children
 *
 * @remark The data will be written in binary format.
 *
 * @param fp File pointer for file we want to write data to.
 * @param node The node whose stats will be written.
 * @pre fp is non-NULL
 * @pre node is non-NULL
 */
static void writeNodeStats(FILE* fp, ProfileNode* node) {
	assert(fp != NULL);
	assert(node != NULL);

	MSG(DEBUG_CREGION, "dyn_id: %llx, static_id: %llx callsite_id: %llx, node_type: %d, num_instances: %llu nChildren: %u DOALL: %llu\n", 
		node->id, node->static_id, node->callsite_id, node->node_type, 
		node->num_instances, node->children.size(), node->is_doall);

	fwrite(&node->id, sizeof(Int64), 1, fp);
	fwrite(&node->static_id, sizeof(Int64), 1, fp);
	fwrite(&node->callsite_id, sizeof(Int64), 1, fp);

	assert(node->node_type >=0 && node->node_type <= 2);
	UInt64 nodeType = node->node_type;
	fwrite(&nodeType, sizeof(Int64), 1, fp);
	
	UInt64 target_id = (node->recursion == NULL) ? 0 : node->recursion->id;
	fwrite(&target_id, sizeof(Int64), 1, fp);
	fwrite(&node->num_instances, sizeof(Int64), 1, fp);
	fwrite(&node->is_doall, sizeof(Int64), 1, fp);
	UInt64 num_children = node->children.size();
	fwrite(&num_children, sizeof(Int64), 1, fp);

	// TRICKY: not sure this is necessary but we go in reverse order to mimic
	// the behavior when we had a C linked-list for children
	for (int i = num_children-1; i >= 0; --i) {
		ProfileNode* child = node->children[i];
		fwrite(&child->id, sizeof(Int64), 1, fp);    
	}

	numCreated++;
}

/*!
 * Write statistics in a given ProfileNodeStats to a specified file.
 *
 * @remark A total of 64 bytes will be written to the file, in the following
 * order:
 *
 * - 64bit work
 * - 64bit total_par_per_work (work after total-parallelism is applied)
 * - 64bit self_par_per_work (work after self-parallelism is applied)
 * - 64bit minimum SP
 * - 64bit maximum SP
 * - 64bit total iteration count
 * - 64bit min iteration count
 * - 64bit max iteration count
 *
 * @remark The data will be written in binary format.
 *
 * @param fp File pointer for file we want to write data to.
 * @param node The ProfileNodeStats whose contents will be written.
 * @pre fp is non-NULL
 * @pre stat is non-NULL
 */
static void emitStat(FILE *fp, ProfileNodeStats *stat) {
	assert(fp != NULL);
	assert(stat != NULL);

	MSG(DEBUG_CREGION, "\tstat: work = %llu, spWork = %llu, nInstance = %llu\n", 
		stat->total_work, stat->self_par_per_work, stat->num_instances);
		
	fwrite(&stat->num_instances, sizeof(Int64), 1, fp);
	fwrite(&stat->total_work, sizeof(Int64), 1, fp);
	fwrite(&stat->total_par_per_work, sizeof(Int64), 1, fp);
	fwrite(&stat->self_par_per_work, sizeof(Int64), 1, fp);

	UInt64 minSPInt = (UInt64)(stat->min_self_par * 100.0);
	UInt64 maxSPInt = (UInt64)(stat->max_self_par * 100.0);
	fwrite(&minSPInt, sizeof(Int64), 1, fp);
	fwrite(&maxSPInt, sizeof(Int64), 1, fp);

	fwrite(&stat->num_dynamic_child_regions, sizeof(Int64), 1, fp);
	fwrite(&stat->min_dynamic_child_regions, sizeof(Int64), 1, fp);
	fwrite(&stat->max_dynamic_child_regions, sizeof(Int64), 1, fp);
}

/*!
 * Write stats for a region--including all children--as long as the region is
 * within the range of depths we are profiling.
 *
 * For each region, the output format is:
 *  - Node Info (writeNodeStats)
 *  - N (64bit), which is # of stats
 *  - N * Stat Info (emitStat)
 *
 * @remark The data will be written in binary format.
 * 
 * @param fp File pointer for file we want to write data to.
 * @param node The node whose stats will be written.
 * @param level The depth in the region tree of the node.
 * @pre fp is non-NULL
 * @pre node is non-NULL
 * @pre There is at least one ProfileNodeStats associated with the node.
 */
static void writeRegionStats(FILE *fp, ProfileNode *node, UInt level) {
    assert(fp != NULL);
    assert(node != NULL);
	assert(node->getStatSize() > 0);

	UInt64 stat_size = node->getStatSize();
	MSG(DEBUG_CREGION, "Emitting Node %llu with %llu stats\n", node->id, stat_size);
	
	if (isEmittable(level)) {
		numEntries++;
		if(node->children.empty())  
			numEntriesLeaf++; 

		writeNodeStats(fp, node);

		fwrite(&stat_size, sizeof(Int64), 1, fp);
		// FIXME: run through stats in reverse?
		for (unsigned i = 0; i < stat_size; ++i) {
			ProfileNodeStats *s = node->stats[i];
			emitStat(fp, s);	
		}
	}

	// TRICKY: not sure this is necessary but we go in reverse order to mimic
	// the behavior when we had a C linked-list for children
	for (int i = node->children.size()-1; i >= 0; --i) {
		ProfileNode* child = node->children[i];
		writeRegionStats(fp, child, level+1);
	}
}

#if 0
void emitDOT(FILE* fp, ProfileNode* node) {
	fprintf(stderr,"DOT: visiting %llu\n",node->id);

	// TRICKY: not sure this is necessary but we go in reverse order to mimic
	// the behavior when we had a C linked-list for children
	for (int i = node->children.size()-1; i >= 0; --i) {
		ProfileNode* child = node->children[i];
		fprintf(fp, "\t%llx -> %llx;\n", node->id, child->id);
		emitDOT(fp, child);
	}
}
#endif



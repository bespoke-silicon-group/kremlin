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

class CStat;

/*!
 * @brief A class to represent a profiled program region.
 */
class CNode {
private:
	const RegionType region_type; /*!< The type of the region associated
										with this node. */

public:
	const UInt64 id; /*!< A unique identifier amongst all CNodes. */
	const UInt64 static_id; /*!< The static ID of the region associated with 
									this node. */
	const UInt64 callsite_id; /*!< The callsite ID of the region associated with 
									this node (or 0 if this is not a function 
									region). */
	UInt64 num_instances; /*!< The number of dynamic instances of
								the region associated with this node. */
	UInt64 is_doall; /*!< Indicates whether the region associated with this
							node is a DOALL region (i.e. completely parallel) */

	CNodeType node_type; /*!< The type of node. */
	CNode *recursion; /*!< Points to node which this is a recursive instance of
							or NULL if this is not a recursive region. */

	// statistics for node
	std::vector<CStat*, MPoolLib::PoolAllocator<CStat*> > stats;
	int curr_stat_index;

	// management of tree
	CNode *parent; /*!< The parent node of this node. */
	std::vector<CNode*, MPoolLib::PoolAllocator<CNode*> > children;

	CNode(SID static_id, CID callsite_id, RegionType type);
	~CNode();

	const RegionType getRegionType() { return region_type; }
	unsigned getStatSize() { return stats.size(); }
	unsigned getNumChildren() { return children.size(); }

	/*!
	 * Returns a string representation of this node.
	 */
	const char* toString();

	/*!
	 * Returns the child with the specified static and callsite ID. If no children
	 * match these criteria, NULL is returns.
	 *
	 * @remark Only function regions have a callsite_id. If a region is not a
	 * function, the match will only be based on the static_id.
	 *
	 * @param static_id The static region ID of the child to find.
	 * @param callsite_id The callsite ID of the child to find. This is ignored if
	 * the child is not a function region.
	 * @return Pointer to the child node with the given static and callsite ID;
	 * NULL if no children match.
	 */
	CNode* getChild(UInt64 static_id, UInt64 callsite_id);

	/*!
	 * Adds a region to this node's list of children. The child's parent will be
	 * set to this node.
	 *
	 * @param child The region to add as a child.
	 * @pre child is non-NULL.
	 * @post The child's parent will be this node.
	 * @post There will be at least one child.
	 */
	void addChild(CNode *child); 

	/*
	 * Move on to the next CStat for this node. If the stat index for this
	 * node was already at the end of the list of this node's CStats, a new
	 * CStat will be created and appended to the end of the list.
	 *
	 * @pre There will be either no CStats or the curr_stat_index will be -1
	 * @post The current stat index will be non-negative.
	 */
	void moveToNextCStat();

	/*
	 * Move on to the previous CStat for this node.
	 *
	 * @pre The current stat index should be non-negative.
	 */
	void moveToPrevCStat();

	/*!
	 * Adds a new set of stats to this node.
	 *
	 * @param new_stats The stats to add.
	 * @pre new_stats is non-NULL.
	 * @post num_instances > 0
	 */
	void addStats(RegionStats *new_stats);

	/*!
	 * Returns the closest ancestor with the same static region ID. If no such ancestor
	 * is found, returns NULL. Note that the root of the region tree should never be
	 * returned as it is a dummy node.
	 *
	 * @return The ancestor with the same static ID as this node; NULL if none exist.
	 * @pre This node has a parent (i.e. isn't a root node).
	 * @post The returned node isn't the root.
	 */
	CNode* getAncestorWithSameStaticID();

	/*!
	 * Checks if this node is a recursive instance of an already existing node.
	 * If so, we modify the nodes involved to indicate the presence of
	 * recursion.
	 */
	void handleRecursion(); 

	static void* operator new(size_t size);
	static void operator delete(void* ptr);
	
private:
	void updateCurrentCStat(RegionStats *info);
	static UInt64 allocId();
};

#endif

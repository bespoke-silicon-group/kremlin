#ifndef _CREGION_H_
#define _CREGION_H_

#include "ktypes.h"

struct RegionStats {
	UInt64 work;
	UInt64 cp;
	UInt64 callSite;
	UInt64 spWork;
	UInt64 is_doall;
	UInt64 childCnt;
#ifdef EXTRA_STATS
	UInt64 readCnt;
	UInt64 writeCnt;
	UInt64 readLineCnt;
	UInt64 writeLineCnt;
	UInt64 loadCnt;
	UInt64 storeCnt;
#endif
};

/*!
 * Initializes the region tree by creating a "dummy" node to act as the root
 * and setting the current region to this new node.
 *
 * @pre The root of the region tree is NULL.
 * @post The region tree will contain a single entry (the root), which will be
 * non-NULL.
 * @post The currently active region will point to the root node.
 */
void initRegionTree();

/*!
 * Deinitializes region tree by removing the root of the tree.
 *
 * @pre The region tree root is non NULL.
 * @pre The current region it the tree root.
 * @post Both the root and the current node will be NULL.
 */
void deinitRegionTree();

/*!
 * Writes profiled data to file whose location is specified.
 *
 * @param filename A string encoding the filename where the profiling data
 * will be written.
 * @pre filename is non-NULL.
 * @pre The region tree root is not NULL (i.e. region tree has been init'ed)
 * @pre No regions are currently on the stack.
 */
void printProfiledData(const char* filename);

/*!
 * Updates the profiled region tree based on entering a program region.
 *
 * @param region_static_id The static ID of the region being entered.
 * @param region_callsite_id The callsite ID of the region being entered.
 * @param region_type The type of region that is being entered.
 *
 * @pre The region tree has been initialized (i.e. root is non-NULL).
 * @pre The current region node exists (i.e. is non NULL).
 * @post The region stack will not be empty.
 * @post The newly entered region's stat index will be non-negative.
 * @post The region stack's size will have increased by one.
 * @invariant curr_region_node will never be an R_SINK node after this
 * function exits.
 */
void openRegionContext(SID region_static_id, CID region_callsite_id, 
						RegionType region_type);

/*!
 * Updates region tree based on exiting the current region.
 *
 * @param region_stats Profile stats for the region that was just exited.
 *
 * @pre region_stats is non-NULL
 * @pre The region stack is not empty.
 * @pre The region tree has been initialized (i.e. root is non-NULL)
 * @pre The current region node exists (i.e. is non-NULL)
 * @pre The current region is not the tree root.
 * @post The region stack's size will have decreased by one.
 */
void closeRegionContext(RegionStats *info);

#endif

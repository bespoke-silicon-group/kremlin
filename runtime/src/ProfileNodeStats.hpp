#ifndef _PROFILENODESTATS_HPP_
#define _PROFILENODESTATS_HPP_

#include "ktypes.h"

/*!
 * @brief Statistics for a profiled region.
 */
class ProfileNodeStats {
public:
	uint64_t total_work; //!< Total amount of work across all instances.
	double min_self_par; //!< Minimum self-parallelism in any instance.
	double max_self_par; //!< Maximum self-parallelism in any instance.

	uint64_t self_par_per_work; //!< work / self-parallelism
	uint64_t total_par_per_work; //!< work / total-parallelism

#ifdef EXTRA_STATS
	uint64_t readCnt;
	uint64_t writeCnt;
	uint64_t loadCnt;
	uint64_t storeCnt;
#endif
	
	uint64_t num_dynamic_child_regions; //!< Total number of dynamic children.
	uint64_t min_dynamic_child_regions; //!< Min dynamic children in any instance.
	uint64_t max_dynamic_child_regions; //!< Max dynamic children in any instance.

	uint64_t num_instances; //!< Total number of instances.

	ProfileNodeStats() : total_work(0), min_self_par(-1), max_self_par(0),
				self_par_per_work(0), total_par_per_work(0), 
#ifdef EXTRA_STATS
				readCnt(0), writeCnt(0), loadCnt(0), storeCnt(0), 
#endif
				num_dynamic_child_regions(0), min_dynamic_child_regions(-1),
				max_dynamic_child_regions(0), num_instances(0) {}
	~ProfileNodeStats() {}

	static void* operator new(size_t size);
	static void operator delete(void *ptr);
};

#endif // _PROFILENODESTATS_HPP_

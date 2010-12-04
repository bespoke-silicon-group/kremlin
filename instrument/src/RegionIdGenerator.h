#ifndef REGION_ID_GENERATOR_H
#define REGION_ID_GENERATOR_H

#include <string>
#include "Region.h"

/**
 * Interface to generate RegionIds.
 */
class RegionIdGenerator
{
	public:

	/**
	 * Returns a unique RegionId for name.
	 *
	 * @param name The name of the region.
	 * @return A unique id for the region.
	 */
	virtual RegionId operator()(const std::string& name) = 0;
};

#endif // REGION_ID_GENERATOR_H

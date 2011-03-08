#ifndef HASH_REGION_ID_GENERATOR_H
#define HASH_REGION_ID_GENERATOR_H

#include <string>
#include "RegionIdGenerator.h"

class HashRegionIdGenerator : public RegionIdGenerator
{
	public:
	HashRegionIdGenerator();
	virtual ~HashRegionIdGenerator();

	virtual RegionId operator()(const std::string& name);
};

#endif // HASH_REGION_ID_GENERATOR_H

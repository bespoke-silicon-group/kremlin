#ifndef HASH_REGION_ID_GENERATOR_H
#define HASH_REGION_ID_GENERATOR_H

#include <string>
#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include "RegionIdGenerator.h"

class HashRegionIdGenerator : public RegionIdGenerator
{
	static boost::uuids::uuid namespaceId;

	boost::uuids::name_generator generator;

	public:
	HashRegionIdGenerator();
	virtual ~HashRegionIdGenerator();

	virtual RegionId operator()(const std::string& name);
};

#endif // HASH_REGION_ID_GENERATOR_H

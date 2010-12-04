#include "HashRegionIdGenerator.h"
#include <boost/static_assert.hpp>

// uuid must be at least the size of RegionId
BOOST_STATIC_ASSERT(sizeof(boost::uuids::uuid) >= sizeof(RegionId));

boost::uuids::uuid HashRegionIdGenerator::namespaceId = {0x77};

HashRegionIdGenerator::HashRegionIdGenerator() : generator(namespaceId)
{
}

HashRegionIdGenerator::~HashRegionIdGenerator()
{
}

RegionId HashRegionIdGenerator::operator()(const std::string& name) 
{
	boost::uuids::uuid id = generator.operator()(name.c_str());

	RegionId region_id = 0;

	// return the XOR of the bits.
	for(size_t i = 0; i < sizeof(boost::uuids::uuid) / sizeof(RegionId); i++)
		region_id ^= ((RegionId*)id.data)[i];

	return region_id;
}


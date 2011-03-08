#include "HashRegionIdGenerator.h"
#include <boost/static_assert.hpp>
#include "ids/UuidToIntAdapter.h"
#include "ids/NameToUuid.h"

// uuid must be at least the size of RegionId
BOOST_STATIC_ASSERT(sizeof(boost::uuids::uuid) >= sizeof(RegionId));

HashRegionIdGenerator::HashRegionIdGenerator()
{
}

HashRegionIdGenerator::~HashRegionIdGenerator()
{
}

RegionId HashRegionIdGenerator::operator()(const std::string& name) 
{
	boost::uuids::uuid id = NameToUuid::generate(name);

    RegionId ret = UuidToIntAdapter<RegionId>::get(id);

	return ret;
}


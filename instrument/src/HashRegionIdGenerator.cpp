#include "HashRegionIdGenerator.h"
#include <boost/static_assert.hpp>
#include "UuidToIntAdapter.h"

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

    RegionId ret = UuidToIntAdapter<RegionId>::get(id);

	// XXX FIXME: this is an ugly hack needed since Java doesn't support unsigned 64 bit nums.
	// This should go away when DJ implements support in the planner (which uses Java) for
	// unsigned 64 bit.
	ret &= 0x7FFFFFFFFFFFFFFF;

	return ret;
}


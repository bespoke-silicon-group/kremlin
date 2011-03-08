#include <boost/uuid/name_generator.hpp>

#include "NameToUuid.h"

boost::uuids::uuid NameToUuid::namespaceId = {0x77};

boost::uuids::uuid NameToUuid::generate(const std::string& name)
{
    boost::uuids::name_generator generator(namespaceId);
    return generator(name);
}

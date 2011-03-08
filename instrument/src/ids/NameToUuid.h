#ifndef NAME_TO_UUID_H
#define NAME_TO_UUID_H

#include <boost/uuid/uuid.hpp>
#include <string>

class NameToUuid
{
    static boost::uuids::uuid namespaceId;

    public:
    static boost::uuids::uuid generate(const std::string& name);
};

#endif // NAME_TO_UUID_H


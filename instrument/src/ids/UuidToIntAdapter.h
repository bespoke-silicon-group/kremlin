#ifndef UUID_TO_INT_ADAPTER_H
#define UUID_TO_INT_ADAPTER_H

#include <boost/uuid/uuid.hpp>

template <typename Integer>
struct UuidToIntAdapter
{
    static Integer get(const boost::uuids::uuid& id);
};

#include "UuidToIntAdapter.tcc"

#endif // UUID_TO_INT_ADAPTER_H

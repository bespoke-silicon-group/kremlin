#include "ids/InstIds.h"

/**
 * Initializes.
 */
InstIds::InstIds() :
    inst_ids(1)
{
}

InstIds::~InstIds()
{
}

/**
 * @return The current count for Ids. This does not account for all ids.
 */
size_t InstIds::getCount() const
{
    return inst_ids;
}

InstIds::IdMap InstIds::getIdMap() {
	return inst_to_id;
}

/**
 * @param inst The inst to get the id of.
 * @return The id for the value. If the value hasn't been assigned one, it
 * will be assigned an unique id.
 */
unsigned int InstIds::getId(const llvm::Value& inst) 
{
    IdMap::const_iterator it = inst_to_id.find(&inst);
    if(it == inst_to_id.end())
    {
        uint64_t id = inst_ids++;
        inst_to_id[&inst] = id;
        return id;
    }
    return it->second;
}



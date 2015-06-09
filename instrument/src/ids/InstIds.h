#ifndef INST_IDS_H
#define INST_IDS_H

#include <llvm/IR/Value.h>
#include <map>

class InstIds
{
    public:
    typedef std::map<const llvm::Value*, unsigned int> IdMap;

    InstIds();
    virtual ~InstIds();

    unsigned int getId(const llvm::Value& inst);
    size_t getCount() const;

	IdMap getIdMap();

    private:

    IdMap inst_to_id;
    uint64_t inst_ids;
};

#endif // INST_IDS_H

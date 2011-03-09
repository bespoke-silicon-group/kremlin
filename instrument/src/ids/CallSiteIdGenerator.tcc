#include <llvm/ADT/StringRef.h>

#include "UuidToIntAdapter.h"
#include "NameToUuid.h"
#include <sstream>
    
template <typename Callable>
uint64_t CallSiteIdGenerator::generate(Callable* callable, uint64_t bb_call_idx)
{
    llvm::StringRef bb_name = callable->getParent()->getName();
    llvm::StringRef func_name = callable->getParent()->getParent()->getName();
    const std::string& module_name = callable->getParent()->getParent()->getParent()->getModuleIdentifier();

    std::ostringstream os;
    os << module_name << func_name.str() << bb_name.str() << bb_call_idx;

    boost::uuids::uuid id = NameToUuid::generate(os.str());
    return UuidToIntAdapter<uint64_t>::get(id);
}

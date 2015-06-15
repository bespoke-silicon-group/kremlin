#include <foreach.h>
#include "GetTerminator.h"
#include <llvm/IR/Instructions.h>

using namespace llvm;

Instruction* GetTerminator::operator()(BasicBlock* bb) const 
{
    Instruction* terminator = bb->getTerminator();
    if(isa<UnreachableInst>(terminator)) {

        // Try call before terminator as the end.
        BasicBlock::InstListType& insts = bb->getInstList();
        if(insts.size() > 1) {

            BasicBlock::InstListType::reverse_iterator inst_before_term_it = insts.rbegin();
            inst_before_term_it++;
            Instruction* inst_before_term = &*inst_before_term_it;

            if(isa<CallInst>(inst_before_term) || isa<InvokeInst>(inst_before_term))
                terminator = inst_before_term;
        }

        // Try the function that doesn't return.
        foreach(Instruction& i, *bb) {
            if(CallInst* ci = dyn_cast<CallInst>(&i)) {
                if(ci->doesNotReturn()) {
                    terminator = ci;
                    break;
                }
            }
        }
    }
    return terminator;
}

#include <llvm/Instructions.h>
#include "analysis/timestamp/ValueClassifier.h"

using namespace llvm;
using namespace std;

/**
 * Constructs a new value classifier.
 */
ValueClassifier::ValueClassifier(ReductionVars& rv) :
    rv(rv)
{
}

/**
 * Returns the classification of the value. These are used to determine which
 * handler to use.
 */
ValueClassifier::Class ValueClassifier::operator()(llvm::Value* val) const
{
    Instruction* inst = dyn_cast<Instruction>(val);
    if(rv.isReductionVar(inst))
        return CONSTANT;

    if(isa<AllocaInst>(val) || isa<Argument>(val) || isa<LoadInst>(val) || isa<PHINode>(val) || isa<CallInst>(val))
        return LIVE_IN;
    if(isa<BinaryOperator>(val) ||
        isa<BitCastInst>(val) ||
        isa<CastInst>(val) ||
        isa<CmpInst>(val) ||
        isa<GetElementPtrInst>(val) ||
        isa<ReturnInst>(val) ||
        isa<StoreInst>(val))
        return CONSTANT_WORK_OP;
    return CONSTANT;
}

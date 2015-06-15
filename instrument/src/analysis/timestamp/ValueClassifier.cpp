#include <llvm/IR/Instructions.h>
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
		// @TRICKY: selects are like an if/else rolled into one instruction.
		// This means they have some implicit phi which we don't handle. As a
		// result of the implicit phi, we are a bit too conservative since
		// only one of the operands (true or false) will be "taken" and
		// therefore only one of them should be a data dependence.
        isa<SelectInst>(val) ||
        isa<GetElementPtrInst>(val) ||
        isa<ReturnInst>(val) ||
        isa<StoreInst>(val))
        return CONSTANT_WORK_OP;
    return CONSTANT;
}

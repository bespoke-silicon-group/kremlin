#define DEBUG_TYPE __FILE__

#include <llvm/Support/Debug.h>
#include <llvm/Module.h>
#include "analysis/WorkAnalysis.h"
#include "LLVMTypes.h"

using namespace llvm;
using namespace std;

/**
 * Constructs a new handler.
 *
 * @param timestamp_placer The placer this handler is associated with.
 * @param work_handler Calculates the work of instructions.
 */
WorkAnalysis::WorkAnalysis(TimestampPlacer& timestamp_placer, const ConstantWorkOpHandler& work_handler) :
    _timestampPlacer(timestamp_placer),
    _workHandler(work_handler)
{
    std::vector<const Type*> arg_types;
    Module& m = *_timestampPlacer.getFunc().getParent();
    LLVMTypes types(m.getContext());

    arg_types.push_back(types.i32()); // work
    FunctionType* func_type = FunctionType::get(types.voidTy(), arg_types, false);

    // if the cast fails, another func with the same name and different prototype exists.
    _instrumentationFunc = cast<Function>(m.getOrInsertFunction("_KWork", func_type)); 
}

/**
 * @param bb The basic block to get the work of.
 * @return The work of the basic block.
 */
uint64_t WorkAnalysis::getWork(BasicBlock& bb) const
{
    uint64_t work = 0;
    foreach(Instruction& inst, bb)
    {
        work += _workHandler.getWork(&inst);

        DEBUG(LOG_DEBUG() << inst << " work: " << _workHandler.getWork(&inst) << "\n");
    }

    return work;
}

/**
 * Handles instrumenting a basic block with logWork().
 *
 * @param bb The basic block to instrument.
 */
void WorkAnalysis::handleBasicBlock(llvm::BasicBlock& bb)
{
    int64_t work_in_bb = getWork(bb);
    if(work_in_bb == 0) return;

    LLVMTypes types(bb.getContext());
    vector<Value*> call_args;
    call_args.push_back(ConstantInt::get(types.i32(), work_in_bb, false));
    CallInst& func_call = *CallInst::Create(_instrumentationFunc, call_args.begin(), call_args.end(), "");

	// Place at the beginning of basic block to avoid cp > work errors during
	// runtime.
    _timestampPlacer.constrainInstPlacement(func_call, *bb.getFirstNonPHI());
}

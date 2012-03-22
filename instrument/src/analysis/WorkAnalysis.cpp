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
 * @param ts_placer The placer this handler is associated with.
 * @param work_handler Calculates the work of instructions.
 */
WorkAnalysis::WorkAnalysis(TimestampPlacer& ts_placer, const ConstantWorkOpHandler& work_handler) :
    ts_placer(ts_placer),
    work_handler(work_handler)
{
    std::vector<const Type*> args;
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());

    args.push_back(types.i32()); // work
    FunctionType* func_type = FunctionType::get(types.voidTy(), args, false);

    // if the cast fails, another func with the same name and different prototype exists.
    log_func = cast<Function>(m.getOrInsertFunction("_KWork", func_type)); 
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
        work += work_handler.getWork(&inst);

        DEBUG(LOG_DEBUG() << inst << " work: " << work_handler.getWork(&inst) << "\n");
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
    vector<Value*> args;
    LLVMTypes types(bb.getContext());

    int64_t work = getWork(bb);

    if(!work)
        return;

    args.push_back(ConstantInt::get(types.i32(), work, false));
    CallInst& ci = *CallInst::Create(log_func, args.begin(), args.end(), "");
    //ts_placer.add(ci, *bb.getTerminator());
    ts_placer.add(ci, *bb.getFirstNonPHI());
}

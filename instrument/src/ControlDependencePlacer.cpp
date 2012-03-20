#include <boost/assign/std/vector.hpp>
#include <boost/assign/std/set.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include "ControlDependencePlacer.h"
#include "LLVMTypes.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

/**
 * Constructs a new control dependence placer.
 *
 * @param ts_placer The placer this handler is associated with.
 */
ControlDependencePlacer::ControlDependencePlacer(TimestampPlacer& ts_placer) :
    cd(ts_placer.getAnalyses().cd),
    ts_placer(ts_placer)
{
    // Setup the add_func
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> args;

    args += types.i32();
    FunctionType* add_type = FunctionType::get(types.voidTy(), args, false);
    add_func = cast<Function>(m.getOrInsertFunction("_KPushCDep", add_type));

    args.clear();
    FunctionType* remove_type = FunctionType::get(types.voidTy(), args, false);
    remove_func = cast<Function>(m.getOrInsertFunction("_KPopCDep", remove_type));
}

/**
 * Adds the add/remove control dependence calls.
 *
 * @param bb The block to add the calls to.
 */
void ControlDependencePlacer::handleBasicBlock(llvm::BasicBlock& bb)
{
    BasicBlock* controller = cd.getControllingBlock(&bb, false);
    if(controller)
    {
        Value& ctrl_dep = *cd.getControllingCondition(controller);
        LLVMTypes types(bb.getContext());
        vector<Value*> args;

        args.clear();
        CallInst& remove_call = *CallInst::Create(remove_func, args.begin(), args.end(), "");
        ts_placer.add(remove_call, *bb.getTerminator());

        set<Instruction*> deps;
        deps += bb.getFirstNonPHI(), &remove_call;
        args += ConstantInt::get(types.i32(), ts_placer.getId(ctrl_dep), false);
        CallInst& add_call = *CallInst::Create(add_func, args.begin(), args.end(), "");
        ts_placer.add(add_call, deps);

        ts_placer.requestTimestamp(ctrl_dep, add_call);
    }

    // Branch conditions technically are live out still.
    Value* this_block_ctrl_val = cd.getControllingCondition(&bb);
    if(this_block_ctrl_val && !isa<Constant>(this_block_ctrl_val))
        ts_placer.requestTimestamp(*this_block_ctrl_val, *bb.getTerminator());
}

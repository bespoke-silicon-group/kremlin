#include <boost/assign/std/vector.hpp>
#include <boost/assign/std/set.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Constants.h>
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
    vector<Type*> args;

    args += types.i32();
	ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
    FunctionType* add_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;
	aref = NULL;
    add_func = cast<Function>(m.getOrInsertFunction("_KPushCDep", add_type));

    args.clear();
    FunctionType* remove_type = FunctionType::get(types.voidTy(), false);
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
        CallInst& remove_call = *CallInst::Create(remove_func, "");
        ts_placer.constrainInstPlacement(remove_call, *bb.getTerminator());

        set<Instruction*> deps;
        deps += bb.getFirstNonPHI(), &remove_call;
        args += ConstantInt::get(types.i32(), ts_placer.getId(ctrl_dep), false);
		ArrayRef<Value*> *aref = new ArrayRef<Value*>(args);
        CallInst& add_call = *CallInst::Create(add_func, *aref, "");
		delete aref;
        ts_placer.constrainInstPlacement(add_call, deps);

        ts_placer.requireValTimestampBeforeUser(ctrl_dep, add_call);
    }

    // Branch conditions technically are live out still.
    Value* this_block_ctrl_val = cd.getControllingCondition(&bb);
    if(this_block_ctrl_val && !isa<Constant>(this_block_ctrl_val))
        ts_placer.requireValTimestampBeforeUser(*this_block_ctrl_val, *bb.getTerminator());
}

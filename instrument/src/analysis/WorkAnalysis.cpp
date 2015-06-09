#define DEBUG_TYPE __FILE__

#include <llvm/Support/Debug.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
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
    std::vector<Type*> arg_types;
    Module& m = *_timestampPlacer.getFunc().getParent();
    LLVMTypes types(m.getContext());

    arg_types.push_back(types.i32()); // work
	ArrayRef<Type*> *aref = new ArrayRef<Type*>(arg_types);
    FunctionType* func_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;

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
	if (bb.isLandingPad() 
		|| dyn_cast<ResumeInst>(bb.getTerminator()) != NULL) return;

    int64_t work_in_bb = getWork(bb);
    if(work_in_bb == 0) return;

    LLVMTypes types(bb.getContext());
    vector<Value*> call_args;

	Module *m = bb.getParent()->getParent();
	GlobalVariable *timetick = m->getGlobalVariable("timetick");

	if(timetick == NULL) {
    	timetick = new GlobalVariable(*m, types.i64(), false, GlobalValue::ExternalLinkage, NULL, "timetick");
	}

	// The following code cuts down on the number of function calls (namely
	// KWork). In limited testing, it didn't seem to have much of an impact so
	// to make the code cleaner (and more compact), we're disabling this for
	// now. If you enable it, you must also make the timetick variable in
	// runtime/src/kremlin.c be a non-static variable.
#if 0
	LoadInst* timetick_load = new LoadInst(timetick);
	BinaryOperator *timetick_add = BinaryOperator::Create(Instruction::Add,timetick_load,
	 ConstantInt::get(types.i64(), work_in_bb));
	StoreInst* timetick_store = new StoreInst(timetick_add,timetick);

    _timestampPlacer.constrainInstPlacement(*timetick_store, *bb.getFirstNonPHI());
    _timestampPlacer.constrainInstPlacement(*timetick_add,*timetick_store);
    _timestampPlacer.constrainInstPlacement(*timetick_load,*timetick_add);
#endif

    call_args.push_back(ConstantInt::get(types.i32(), work_in_bb, false));
	ArrayRef<Value*> *aref = new ArrayRef<Value*>(call_args);
    CallInst& func_call = *CallInst::Create(_instrumentationFunc, *aref, "");
	delete aref;

	// Place at the beginning of basic block to avoid cp > work errors during
	// runtime.
    _timestampPlacer.constrainInstPlacement(func_call, *bb.getFirstNonPHI());
}

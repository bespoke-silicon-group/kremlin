#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include "ReturnHandler.h"
#include "LLVMTypes.h"
#include "ReturnsRealValue.h"

using namespace llvm;
using namespace std;

/**
 * Constructs a new handler.
 *
 * @param ts_placer The placer this handler is associated with.
 */
ReturnHandler::ReturnHandler(TimestampPlacer& ts_placer) :
    ts_placer(ts_placer)
{
    opcodes.push_back(Instruction::Ret);

    // Setup the ret_const_func
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<Type*> args;

    FunctionType* ret_const_type = FunctionType::get(types.voidTy(), false);
    ret_const_func = cast<Function>(m.getOrInsertFunction("_KReturnConst", ret_const_type));

    // Setup the ret_func
    args.push_back(types.i32());
	ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
    FunctionType* ret_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;
    ret_func = cast<Function>(m.getOrInsertFunction("_KReturn", ret_type));
}

/**
 * @copydoc TimestampPlacerHandler::getOpcodes()
 */
const TimestampPlacerHandler::Opcodes& ReturnHandler::getOpcodes()
{
    return opcodes;
}

/**
 * @copydoc TimestampPlacerHandler::handle()
 */
void ReturnHandler::handle(llvm::Instruction& inst)
{
    ReturnInst& ri = *cast<ReturnInst>(&inst);
    LLVMTypes types(ri.getContext());
    vector<Value*> args;

    ReturnsRealValue ret_real_val;
    if(ret_real_val(ts_placer.getFunc()) && // make sure this returns a non-pointer
        ri.getNumOperands() != 0) // and that it isn't returning void
    {
        Value* ret_val = ri.getReturnValue();
        Function* log_func = ret_const_func;
        if(!isa<Constant>(ret_val)) 
        {
            log_func = ret_func;
            args.push_back(ConstantInt::get(types.i32(), ts_placer.getId(*ret_val), false));
            LOG_DEBUG() << "returning non-const value\n";
        }
		ArrayRef<Value*> *aref = new ArrayRef<Value*>(args);
        CallInst& ci = *CallInst::Create(log_func, *aref, "");
		delete aref;
        ts_placer.constrainInstPlacement(ci, ri);

        if(!isa<Constant>(ret_val)) 
            ts_placer.requireValTimestampBeforeUser(*ret_val, ci);
    }
}

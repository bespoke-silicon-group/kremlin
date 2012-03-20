#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include "ReturnHandler.h"
#include "LLVMTypes.h"
#include "ReturnsRealValue.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

/**
 * Constructs a new handler.
 *
 * @param ts_placer The placer this handler is associated with.
 */
ReturnHandler::ReturnHandler(TimestampPlacer& ts_placer) :
    ts_placer(ts_placer)
{
    opcodes += Instruction::Ret;

    // Setup the ret_const_func
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> args;

    FunctionType* ret_const_type = FunctionType::get(types.voidTy(), args, false);
    ret_const_func = cast<Function>(m.getOrInsertFunction("_KReturnConst", ret_const_type));

    // Setup the ret_func
    args += types.i32();
    FunctionType* ret_type = FunctionType::get(types.voidTy(), args, false);
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
        Value& ret_val = *ri.getReturnValue(0);
        Function* log_func = ret_const_func;
        if(!isa<Constant>(&ret_val)) 
        {
            log_func = ret_func;
            args += ConstantInt::get(types.i32(), ts_placer.getId(ret_val), false);
            LOG_DEBUG() << "returning non-const value\n";
        }
        CallInst& ci = *CallInst::Create(log_func, args.begin(), args.end(), "");
        ts_placer.add(ci, ri);

        if(!isa<Constant>(&ret_val)) 
            ts_placer.requestTimestamp(ret_val, ci);
    }
}

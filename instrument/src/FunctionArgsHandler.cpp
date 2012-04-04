#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include "LLVMTypes.h"
#include "FunctionArgsHandler.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

FunctionArgsHandler::FunctionArgsHandler(TimestampPlacer& ts_placer)
{
    // Setup the log_func
    Function& func = ts_placer.getFunc();
    Module& m = *func.getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> type_args;
    type_args += types.i32();
    FunctionType* func_type = FunctionType::get(types.voidTy(), type_args, false);

    Function& log_func = *cast<Function>(m.getOrInsertFunction("_KUnlinkArg", func_type));

    // Add transfer and unlink to all the arguments passed by value.
	// @TRICKY The exception is for main, which /likely/ isn't called from
	// somewhere and therefore won't have args to unlink. This might cause
	// funkiness in programs that do call main from somewhere inside the
	// program.

	// @TODO This likely needs fixed for Fortran (and maybe C++) programs
	// which has a different name for main
	if(func.hasName() && func.getName().compare("main") == 0) return;

    Instruction* last_inst = func.getEntryBlock().getFirstNonPHI();
    for(Function::arg_iterator arg_it = func.arg_begin(), arg_end = func.arg_end(); arg_it != arg_end; ++arg_it)
    {
        Value& arg = *arg_it;
        vector<Value*> args;
        if(!isa<PointerType>(arg.getType())) 
        {
            args.clear();
            args += ConstantInt::get(types.i32(), ts_placer.getId(arg)); // dest ID

            // insert at the very beginning of the function
            CallInst& ci = *CallInst::Create(&log_func, args.begin(), args.end(), "");
            ts_placer.constrainInstPlacement(ci, *last_inst);
            last_inst = &ci;
        }
    }
}

const TimestampPlacerHandler::Opcodes& FunctionArgsHandler::getOpcodes()
{
    return opcodes;
}

void FunctionArgsHandler::handle(llvm::Instruction& inst)
{
}

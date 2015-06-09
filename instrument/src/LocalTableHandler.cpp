#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include "LLVMTypes.h"
#include "LocalTableHandler.h"

using namespace llvm;
using namespace std;

/**
 * Constructs a new handler for adding in the setupLocalTable calls.
 */
LocalTableHandler::LocalTableHandler(TimestampPlacer& ts_placer, InstIds& inst_ids)
{
    Function& func = ts_placer.getFunc();
    Module& m = *func.getParent();
    LLVMTypes types(m.getContext());
    vector<Type*> type_args;

    type_args.push_back(types.i32());
    type_args.push_back(types.i32());
	ArrayRef<Type*> *type_args_array = new ArrayRef<Type*>(type_args);
    FunctionType* func_type = FunctionType::get(types.voidTy(), *type_args_array, false);
	delete type_args_array;
    Function& log_func = *cast<Function>(m.getOrInsertFunction("_KPrepRTable", func_type));

    vector<Value*> args;
    args.push_back(ConstantInt::get(types.i32(), inst_ids.getCount()));
    args.push_back(ConstantInt::get(types.i32(), 0)); // placeholder
	ArrayRef<Value*> *args_array = new ArrayRef<Value*>(args);
    CallInst& ci = *CallInst::Create(&log_func, *args_array, "");
	delete args_array;
    ts_placer.constrainInstPlacement(ci, *func.getEntryBlock().getFirstNonPHI());
}

const TimestampPlacerHandler::Opcodes& LocalTableHandler::getOpcodes()
{
    return opcodes;
}

void LocalTableHandler::handle(llvm::Instruction& inst) {}

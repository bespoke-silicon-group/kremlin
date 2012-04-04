#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include "LLVMTypes.h"
#include "LocalTableHandler.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

/**
 * Constructs a new handler for adding in the setupLocalTable calls.
 */
LocalTableHandler::LocalTableHandler(TimestampPlacer& ts_placer, InstIds& inst_ids)
{
    Function& func = ts_placer.getFunc();
    Module& m = *func.getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> type_args;

    type_args += types.i32();
    type_args += types.i32();
    FunctionType* func_type = FunctionType::get(types.voidTy(), type_args, false);
    Function& log_func = *cast<Function>(m.getOrInsertFunction("_KPrepRTable", func_type));

    vector<Value*> args;
    args += ConstantInt::get(types.i32(), inst_ids.getCount());
    args += ConstantInt::get(types.i32(), 0); // placeholder
    CallInst& ci = *CallInst::Create(&log_func, args.begin(), args.end(), "");
    ts_placer.constrainInstPlacement(ci, *func.getEntryBlock().getFirstNonPHI());
}

const TimestampPlacerHandler::Opcodes& LocalTableHandler::getOpcodes()
{
    return opcodes;
}

void LocalTableHandler::handle(llvm::Instruction& inst) {}

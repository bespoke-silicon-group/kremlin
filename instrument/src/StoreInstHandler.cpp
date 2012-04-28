#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include "StoreInstHandler.h"
#include "LLVMTypes.h"
#include "MemoryInstHelper.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

/**
 * Constructs a new handler for store instructions.
 */
StoreInstHandler::StoreInstHandler(TimestampPlacer& timestamp_placer) :
    log(PassLog::get()),
    timestampPlacer(timestamp_placer)
{
    // Set up the opcodes
    opcodes += Instruction::Store;

    // Setup the storeRegFunc and storeConstFunc functions
    Module& module = *timestampPlacer.getFunc().getParent();
    LLVMTypes types(module.getContext());

    vector<const Type*> func_param_types;
    func_param_types += types.i32(), types.pi8(), types.i32();
    FunctionType* store_func_type = FunctionType::get(types.voidTy(), func_param_types, false);
    storeRegFunc = cast<Function>(module.getOrInsertFunction("_KStore", store_func_type));

	func_param_types.clear();
    func_param_types += types.pi8(), types.i32();
    FunctionType* store_const_func_type = FunctionType::get(types.voidTy(), func_param_types, false);
    storeConstFunc = cast<Function>(module.getOrInsertFunction("_KStoreConst", store_const_func_type));
}

/**
 * @copydoc TimestampPlacerHandler::getOpcodes()
 */
const std::vector<unsigned int>& StoreInstHandler::getOpcodes()
{
    return opcodes;
}

/**
 * Handles a store instruction. Adds the call to logStoreInst.
 *
 * @param inst The store instruction.
 */
void StoreInstHandler::handle(llvm::Instruction& inst)
{
    LOG_DEBUG() << "handling store\n";

    Module& module = *timestampPlacer.getFunc().getParent();
    LLVMTypes types(module.getContext());
    vector<Value*> call_args;

    StoreInst& store_inst = *cast<StoreInst>(&inst);

    // Get the ID for the source (if we're not storing a constant value)
    Value& src_val = *store_inst.getValueOperand();

	if(!isa<Constant>(src_val))
    	call_args += ConstantInt::get(types.i32(),timestampPlacer.getId(src_val));

	// Destination address is already a pointer; we ust need to cast it to
	// void* (i.e.  i8*) so we don't have to specialize the function based on
	// the size of the pointer.
    CastInst& dest_ptr_cast = *CastInst::CreatePointerCast(store_inst.getPointerOperand(),types.pi8(),"inst_arg_ptr");
    call_args += &dest_ptr_cast;

	// final arg is the memory access size
    call_args += ConstantInt::get(types.i32(),MemoryInstHelper::getTypeSizeInBytes(&src_val));

    // Use the timestamp placer to place the call, the pointer cast, and the
	// timestamp calc (if not storing a constant).
	Function* func_to_call = NULL;
	if(isa<Constant>(src_val))
		func_to_call = storeConstFunc;
	else
		func_to_call = storeRegFunc;

    CallInst& call_inst = *CallInst::Create(func_to_call, call_args.begin(), call_args.end(), "");
    timestampPlacer.constrainInstPlacement(dest_ptr_cast, call_inst);
    timestampPlacer.constrainInstPlacement(call_inst, inst);
	if(!isa<Constant>(src_val))
    	timestampPlacer.requireValTimestampBeforeUser(src_val, call_inst);
}

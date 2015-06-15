#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include "StoreInstHandler.h"
#include "LLVMTypes.h"
#include "MemoryInstHelper.h"

using namespace llvm;
using namespace std;

/**
 * Constructs a new handler for store instructions.
 */
StoreInstHandler::StoreInstHandler(TimestampPlacer& timestamp_placer) :
    log(PassLog::get()),
    timestampPlacer(timestamp_placer)
{
    // Set up the opcodes
    opcodes.push_back(Instruction::Store);

    // Setup the storeRegFunc and storeConstFunc functions
    Module& module = *timestampPlacer.getFunc().getParent();
    LLVMTypes types(module.getContext());

    vector<Type*> func_param_types;
    func_param_types.push_back(types.i32());
    func_param_types.push_back(types.pi8());
    func_param_types.push_back(types.i32());
	ArrayRef<Type*> *aref = new ArrayRef<Type*>(func_param_types);
    FunctionType* store_func_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;
    storeRegFunc = cast<Function>(module.getOrInsertFunction("_KStore", store_func_type));

	func_param_types.clear();
    func_param_types.push_back(types.pi8());
    func_param_types.push_back(types.i32());
	aref = new ArrayRef<Type*>(func_param_types);
    FunctionType* store_const_func_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;
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
    LOG_DEBUG() << "handling: " << inst << "\n";

    Module& module = *timestampPlacer.getFunc().getParent();
    LLVMTypes types(module.getContext());
    vector<Value*> call_args;

    StoreInst& store_inst = *cast<StoreInst>(&inst);

    // Get the ID for the source (if we're not storing a constant value)
    Value& src_val = *store_inst.getValueOperand();

	if(!isa<Constant>(src_val))
    	call_args.push_back(ConstantInt::get(types.i32(),timestampPlacer.getId(src_val)));

	// Destination address is already a pointer; we ust need to cast it to
	// void* (i.e.  i8*) so we don't have to specialize the function based on
	// the size of the pointer.
    CastInst& dest_ptr_cast = *CastInst::CreatePointerCast(store_inst.getPointerOperand(),types.pi8(),"inst_arg_ptr");
    call_args.push_back(&dest_ptr_cast);

	// final arg is the memory access size
    call_args.push_back(ConstantInt::get(types.i32(),MemoryInstHelper::getTypeSizeInBytes(&src_val)));

    // Use the timestamp placer to place the call, the pointer cast, and the
	// timestamp calc (if not storing a constant).
	Function* func_to_call = NULL;
	if(isa<Constant>(src_val))
		func_to_call = storeConstFunc;
	else
		func_to_call = storeRegFunc;

	ArrayRef<Value*> *aref = new ArrayRef<Value*>(call_args);
    CallInst& call_inst = *CallInst::Create(func_to_call, *aref, "");
	delete aref;
    timestampPlacer.constrainInstPlacement(dest_ptr_cast, call_inst);
    timestampPlacer.constrainInstPlacement(call_inst, inst);
	if(!isa<Constant>(src_val))
    	timestampPlacer.requireValTimestampBeforeUser(src_val, call_inst);
}

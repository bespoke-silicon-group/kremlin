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
StoreInstHandler::StoreInstHandler(TimestampPlacer& ts_placer) :
    log(PassLog::get()),
    ts_placer(ts_placer)
{
    // Set up the opcodes
    opcodes += Instruction::Store;

    // Setup the storeRegFunc
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> args;
    args += types.i32(), types.pi8(), types.i32();
    FunctionType* store_func_type = FunctionType::get(types.voidTy(), args, false);

    storeRegFunc = cast<Function>(m.getOrInsertFunction("_KStore", store_func_type));

	args.clear();
    args += types.pi8(), types.i32();
    FunctionType* store_const_func_type = FunctionType::get(types.voidTy(), args, false);
	
    storeConstFunc = cast<Function>(m.getOrInsertFunction("_KStoreConst", store_const_func_type));
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
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());

    vector<Value*> args;
    StoreInst& si = *cast<StoreInst>(&inst);

    LOG_DEBUG() << "inst is a store inst\n";

    // first we get a ptr to the source
    Value& src = *si.getOperand(0);
	if(!isa<Constant>(src))
    	args += ConstantInt::get(types.i32(),ts_placer.getId(src)); // src ID

    // the dest is already in ptr form so we simply use that
    CastInst& cast_inst = *CastInst::CreatePointerCast(si.getPointerOperand(),types.pi8(),"inst_arg_ptr"); // dest addr
    args += &cast_inst;

	// size of access
    args += ConstantInt::get(types.i32(),MemoryInstHelper::getTypeSizeInBytes(&src));

    // Add the cast, call and the timestamp to store.
	Function* proper_func = NULL;
	if(isa<Constant>(src))
		proper_func = storeConstFunc;
	else
		proper_func = storeRegFunc;

    CallInst& ci = *CallInst::Create(proper_func, args.begin(), args.end(), "");
    ts_placer.constrainInstPlacement(cast_inst, ci);
    ts_placer.constrainInstPlacement(ci, inst);
	if(!isa<Constant>(src))
    	ts_placer.requireValTimestampBeforeUser(src, ci);
}

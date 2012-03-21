#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Support/CallSite.h>
#include "LLVMTypes.h"
#include "DynamicMemoryHandler.h"
#include "ReturnsRealValue.h"

// for untangle() function
#include "CallInstHandler.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

DynamicMemoryHandler::DynamicMemoryHandler(TimestampPlacer& ts_placer) :
    call_idx(0),
    log(PassLog::get()),
    ts_placer(ts_placer)
{
    opcodes += Instruction::Call;

    // Setup funcs
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> args;

	args += types.pi8(), types.i64(), types.i32();

    FunctionType* malloc_call = FunctionType::get(types.voidTy(), args, false);
    malloc_func = cast<Function>(
        m.getOrInsertFunction("_KMalloc", malloc_call));

	args.clear();

	args += types.pi8(), types.pi8(), types.i64(), types.i32();
    FunctionType* realloc_call = FunctionType::get(types.voidTy(), args, false);
    realloc_func = cast<Function>(
        m.getOrInsertFunction("_KRealloc", realloc_call));

	args.clear();

	args += types.pi8();
    FunctionType* free_call = FunctionType::get(types.voidTy(), args, false);
    free_func = cast<Function>(
        m.getOrInsertFunction("_KFree", free_call));
}

const TimestampPlacerHandler::Opcodes& DynamicMemoryHandler::getOpcodes()
{
    return opcodes;
}

// Returns true if val is a pointer to n-bit int
bool DynamicMemoryHandler::isNBitIntPointer(Value* val, unsigned n) {
	const Type* val_type = val->getType();

	assert(isa<PointerType>(val_type) && "value is not even a pointer");

	const Type* val_ele_type = cast<PointerType>(val_type)->getElementType();

	return isa<IntegerType>(val_ele_type) && (cast<IntegerType>(val_ele_type)->getBitWidth() == n);
}

Instruction* DynamicMemoryHandler::getNextInst(Instruction *inst) {
	BasicBlock::iterator bb_it = *inst;
	++bb_it;
	return bb_it;
}

void DynamicMemoryHandler::handle(llvm::Instruction& inst)
{
	LOG_DEBUG() << "handling: " << inst << "\n";

    CallInst& call_inst = *cast<CallInst>(&inst);

    LLVMTypes types(call_inst.getContext());
    vector<Value*> args;

	Function *called_func = CallInstHandler::untangleCall(call_inst);

	// calls to malloc and calloc get _KMalloc(addr, size) call
	if(called_func 
	  && (called_func->getName().compare("malloc") == 0 || called_func->getName().compare("calloc") == 0)
	  ) {
		LOG_DEBUG() << "inst is a call to malloc/calloc\n";

		// insert address (return value of callinst)
		LOG_DEBUG() << "adding return value of inst as arg to _KMalloc\n";
		args.push_back(&call_inst);

		// insert size (arg 0 of func)
		Value* sizeOperand = call_inst.getArgOperand(0);
		LOG_DEBUG() << "pushing arg: " << PRINT_VALUE(*sizeOperand) << "\n";
		args.push_back(sizeOperand);

		args.push_back(ConstantInt::get(types.i32(),ts_placer.getId(call_inst))); // dest ID

        CallInst* malloc_call = CallInst::Create(malloc_func, args.begin(), args.end(), "");

		// XXX: this is a slightly hackish way of getting call to KMalloc
		// inserted immediately after the call to malloc
        ts_placer.add(*malloc_call, *getNextInst(&call_inst));
		
		args.clear();
	}

	// calls to free get _KFree(addr) call
	else if(called_func && called_func->getName().compare("free") == 0) {
		LOG_DEBUG() << "inst is a call to free\n";

		// Insert address (first arg of call ist)
		// If op1 isn't pointing to an 8-bit int then we need to cast it to one for use.
		if(isNBitIntPointer(call_inst.getArgOperand(0),8)) {
			args.push_back(call_inst.getArgOperand(0));
		}
		else {
			args.push_back(CastInst::CreatePointerCast(call_inst.getArgOperand(0),types.pi8(),"free_arg_recast",&call_inst));
		}


        CallInst* free_call = CallInst::Create(free_func, args.begin(), args.end(), "");

		// XXX: this is a slightly hackish way of getting call to KFree
		// inserted immediately after the call to free
        ts_placer.add(*free_call, *getNextInst(&call_inst));

		args.clear();
	}

	// handle calls to realloc
	else if(called_func && called_func->getName().compare("realloc") == 0) {
		LOG_DEBUG() << "isnt is  call to realloc\n";

		// Insert old addr (arg 0 of func).
		// Just like for free, we need to make sure this has type i8*
		if(isNBitIntPointer(call_inst.getArgOperand(0),8)) {
			args.push_back(call_inst.getArgOperand(0));
		}
		else {
			args.push_back(CastInst::CreatePointerCast(call_inst.getArgOperand(0),types.pi8(),"realloc_arg_recast",&call_inst));
		}

		// insert new addr (return val of callinst)
		args.push_back(&call_inst);

		// insert size (arg 1 of function)
		args.push_back(call_inst.getArgOperand(1));

		args.push_back(ConstantInt::get(types.i32(),ts_placer.getId(call_inst))); // dest ID

        CallInst* realloc_call = CallInst::Create(realloc_func, args.begin(), args.end(), "");

		// XXX: this is a slightly hackish way of getting call to KRealloc
		// inserted immediately after the call to realloc
        ts_placer.add(*realloc_call, *getNextInst(&call_inst));

		args.clear();
	}

}

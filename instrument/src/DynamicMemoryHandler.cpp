#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Support/CallSite.h>
#include "LLVMTypes.h"
#include "DynamicMemoryHandler.h"
#include "ReturnsRealValue.h"

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

    // link_arg_const
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

// This function tries to untangle some strangely formed function calls.  If
// the call inst is a normal call inst then it just returns the function that
// is returned by ci.getCalledFunction(). Otherwise, it checks to see if the
// first op of the call is a constant bitcast op that can result from LLVM not
// knowing the function declaration ahead of time. If it detects this
// situation, it will grab the function that is being cast and return that.
// TODO: remove this and use the version in CallInstHandler
template <typename Callable>
Function* DynamicMemoryHandler::untangleCall(Callable& ci)
{
    if(ci.getCalledFunction()) { return ci.getCalledFunction(); }

    Value* op0 = ci.getCalledValue(); // TODO: rename this to called_val
    if(!isa<User>(op0)) {
        LOG_DEBUG() << "skipping op0 of callinst because it isn't a User: " << *op0 << "\n";
        return NULL;
    }

    User* user = cast<User>(op0);

    Function* called_func = NULL;

    // check if the user is a constant bitcast expr with a function as the first operand
    if(isa<ConstantExpr>(user)) { 
        if(isa<Function>(user->getOperand(0))) {
            //LOG_DEBUG() << "untangled function ref from bitcast expr\n";
            called_func = cast<Function>(user->getOperand(0));
        }
    }

    return called_func;
}

void DynamicMemoryHandler::handle(llvm::Instruction& inst)
{
    CallInst& call_inst = *cast<CallInst>(&inst);

    LLVMTypes types(call_inst.getContext());
    vector<Value*> args;

	Function *called_func = untangleCall(call_inst);

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

		// XXX: this probably should use the placer system but there currently
		// isn't an easy way to say "place this AFTER said instruction"
        //ts_placer.add(*malloc_call, call_inst.getParent()->getTerminator());
		malloc_call->insertAfter(&call_inst);
		
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

		// XXX: see note above about using placer here
        //ts_placer.add(*free_call, call_inst);
		free_call->insertAfter(&call_inst);

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

		// XXX: see note above about using placer here
        //ts_placer.add(*realloc_call, call_inst);
		realloc_call->insertAfter(&call_inst);
		args.clear();
	}

}

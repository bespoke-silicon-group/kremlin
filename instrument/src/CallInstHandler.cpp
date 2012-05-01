#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Support/CallSite.h>
#include "LLVMTypes.h"
#include "CallInstHandler.h"
#include "ReturnsRealValue.h"
#include "InstrumentedCall.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

CallInstHandler::CallInstHandler(TimestampPlacer& timestamp_placer) :
    callSiteIdCounter(0),
    log(PassLog::get()),
    timestampPlacer(timestamp_placer)
{
    opcodesOfHandledInsts += Instruction::Call;

    // Setup funcs
    Module& m = *timestampPlacer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> func_arg_types;

    FunctionType* link_arg_const_func_type = FunctionType::get(types.voidTy(), func_arg_types, false);
    linkArgConstFunc = cast<Function>(m.getOrInsertFunction("_KLinkArgConst", link_arg_const_func_type));
    
    func_arg_types += types.i32();
    FunctionType* link_arg_func_type = FunctionType::get(types.voidTy(), func_arg_types, false);
    linkArgFunc = cast<Function>(m.getOrInsertFunction("_KLinkArg", link_arg_func_type));

    func_arg_types.clear();
    func_arg_types += types.i64(), types.i64();
    FunctionType* prep_call_func_type = FunctionType::get(types.voidTy(), func_arg_types, false);
    prepCallFunc = cast<Function>(m.getOrInsertFunction("_KPrepCall", prep_call_func_type));

    func_arg_types.clear();
    func_arg_types += types.i32();
    FunctionType* ret_val_link_func_type = FunctionType::get(types.voidTy(), func_arg_types, false);
    linkReturnFunc = cast<Function>(m.getOrInsertFunction("_KLinkReturn", ret_val_link_func_type));
}

const TimestampPlacerHandler::Opcodes& CallInstHandler::getOpcodes()
{
    return opcodesOfHandledInsts;
}

// This function tries to untangle some strangely formed function calls.  If
// the call inst is a normal call inst then it just returns the function that
// is returned by call_inst.getCalledFunction(). Otherwise, it checks to see if the
// first op of the call is a constant bitcast op that can result from LLVM not
// knowing the function declaration ahead of time. If it detects this
// situation, it will grab the function that is being cast and return that.
template <typename Callable>
Function* CallInstHandler::untangleCall(Callable& callable_inst)
{
    if(callable_inst.getCalledFunction()) { return callable_inst.getCalledFunction(); }

	// XXX: not exactly sure what case this if statement handles
    Value *called_val = callable_inst.getCalledValue();
    User *called_user = dyn_cast<User>(called_val);
    if(called_user == NULL) {
        LOG_DEBUG() << "skipping called_val because it isn't a User: " << *called_val << "\n";
        return NULL;
    }

    Function *called_func = NULL;

	// Check if the called_user is a constant bitcast expr with a function as
	// the first operand.
    if(isa<ConstantExpr>(called_user)) { 
        if(isa<Function>(called_user->getOperand(0))) {
            called_func = cast<Function>(called_user->getOperand(0));
        }
    }

    return called_func;
}

void CallInstHandler::addIgnore(string func_name)
{
	ignoredFuncs += func_name;
}

bool CallInstHandler::shouldHandle(Function *func) {
	if (func->isIntrinsic()) return false;
	else if (!func->hasName()) return false;

	foreach(string name, ignoredFuncs) {
		if (func->getName().compare(name) == 0 ) return false;
	}

	return true;
}

void CallInstHandler::handle(llvm::Instruction& inst)
{
	/*
	 * CallInsts require multiple kremlib functions be inserted in a specific
	 * order to ensure that everything works correctly. First, KPrepCall needs
	 * to set up the callsite id, then any arguments not passed by value need
	 * to be linked explicitly using KLinkArg/KLinkArgConst. Finally, if the
	 * call returns a value, we'll explicitly link that using KLinkReturn.
	 * We create these calls in reverse order because we use each call as the
	 * placement constraint for the call before it.
	 */
	LOG_DEBUG() << "handling: " << inst << "\n";
    CallInst& call_inst = *cast<CallInst>(&inst);

    LLVMTypes types(call_inst.getContext());

    Function* raw_called_func = call_inst.getCalledFunction();

	// don't do anything for LLVM instrinsic functions since we know we'll
	// never instrument those functions
    if(raw_called_func && !shouldHandle(raw_called_func))
    {
        LOG_DEBUG() << "ignoring call to function: " << raw_called_func->getName() << "\n";
        return;
    }

    Function* called_func = untangleCall(call_inst);

    if(called_func)
        LOG_DEBUG() << "Call to function: " << called_func->getName() << "\n";
    else
        LOG_DEBUG() << "Call is function pointer\n";

    // Function that pushes an llvm int into kremlib_call_args.
    vector<Value*> kremlib_call_args;
    function<void(const Type*, unsigned int)> push_int = bind(&vector<Value*>::push_back,
        ref(kremlib_call_args), bind<Constant*>(&ConstantInt::get, _1, _2, false));

	// We are going to insert multiple calls to kremlib functions. In order to
	// maintain the correct ordering, we'll use the last_call and feed that as
	// a constraint to the timestamp placer.
    CallInst* last_call = &call_inst;

    // Add in a call to KLinkReturn if the call inst we are handling returns a
	// real value.
    ReturnsRealValue ret_real_val;
    if(ret_real_val(call_inst)) 
    {
        kremlib_call_args.clear();
        push_int(types.i32(), timestampPlacer.getId(call_inst)); // dest ID
        CallInst* ret_val_link = CallInst::Create(linkReturnFunc, kremlib_call_args.begin(), kremlib_call_args.end(), "");
        timestampPlacer.constrainInstPlacement(*ret_val_link, *last_call);
        last_call = ret_val_link;
    }

    // Any arguments not passed by ref will have an implicit copy to the stack. We
	// need to link all those args explicitly using KLinkArg.
    CallSite call_site = CallSite::get(&call_inst);

    for(CallSite::arg_iterator arg_it = call_site.arg_begin(), arg_end = call_site.arg_end(); arg_it != arg_end; ++arg_it) 
    {
        Value& call_arg = **arg_it;
        LOG_DEBUG() << "checking arg: " << call_arg << "\n";

        if(!isa<PointerType>(call_arg.getType())) 
        {
            kremlib_call_args.clear();
            CallInst *link_arg_call = NULL;
            if(!isa<Constant>(&call_arg))
            {
                push_int(types.i32(), timestampPlacer.getId(call_arg)); // Source ID.
                link_arg_call = CallInst::Create(linkArgFunc, kremlib_call_args.begin(), kremlib_call_args.end(), "");

                timestampPlacer.requireValTimestampBeforeUser(call_arg, *link_arg_call);
            }
            else // this is a constant so call linkArgToConst instead (which takes no args)
			{
                link_arg_call = CallInst::Create(linkArgConstFunc, kremlib_call_args.begin(), kremlib_call_args.end(), "");
			}
            timestampPlacer.constrainInstPlacement(*link_arg_call, *last_call);
            last_call = link_arg_call;
        }
    } // end for(arg_it)

    // Create call to KPrepCall. We need two arguments for this call: the
	// callsite id and the caller region ID. Note that the latter is not
	// currently implemented: it's always 0 for now.
    kremlib_call_args.clear();

	InstrumentedCall<CallInst>* instrumented_call = new InstrumentedCall<CallInst>(&call_inst, callSiteIdCounter);
	instrumented_call->instrument();
	callSiteIdCounter++;

	// @TRICKY: push_int doesn't really handle 64-bit ints since its
	// definition uses "unsigned int" instead of "unsigned long long". We
	// therefore have to go about adding the callsite ID and caller region ID
	// the old fashioned way (i.e. with push_back)
	kremlib_call_args.push_back(ConstantInt::get(types.i64(),instrumented_call->getId()));
	kremlib_call_args.push_back(ConstantInt::get(types.i64(),0));
    CallInst& prep_call = *CallInst::Create(prepCallFunc, kremlib_call_args.begin(), kremlib_call_args.end(), "");
    timestampPlacer.constrainInstPlacement(prep_call, *last_call);

	delete instrumented_call;
}
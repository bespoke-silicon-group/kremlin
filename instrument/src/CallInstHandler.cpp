#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Support/CallSite.h>
#include "LLVMTypes.h"
#include "CallInstHandler.h"
#include "ReturnsRealValue.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

CallInstHandler::CallInstHandler(TimestampPlacer& ts_placer) :
    call_idx(0),
    log(PassLog::get()),
    ts_placer(ts_placer)
{
    opcodes += Instruction::Call;

    // Setup funcs
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> args;

    // link_arg_const
    FunctionType* link_arg_const = FunctionType::get(types.voidTy(), args, false);
    link_arg_const_func = cast<Function>(
        m.getOrInsertFunction("_KLinkArgConst", link_arg_const));
    
    // link_arg
    args += types.i32();
    FunctionType* link_arg = FunctionType::get(types.voidTy(), args, false);
    link_arg_func = cast<Function>(
        m.getOrInsertFunction("_KLinkArg", link_arg));

    // prepare_call
    args.clear();
    args += types.i64(), types.i64();
    FunctionType* prepare_args = FunctionType::get(types.voidTy(), args, false);
    prepare_call_func = cast<Function>(
        m.getOrInsertFunction("_KPrepCall", prepare_args));

    // ret_val_link
    args.clear();
    args += types.i32();
    FunctionType* ret_val_link = FunctionType::get(types.voidTy(), args, false);
    ret_val_link_func = cast<Function>(
        m.getOrInsertFunction("_KLinkReturn", ret_val_link));
}

const TimestampPlacerHandler::Opcodes& CallInstHandler::getOpcodes()
{
    return opcodes;
}

// This function tries to untangle some strangely formed function calls.  If
// the call inst is a normal call inst then it just returns the function that
// is returned by ci.getCalledFunction(). Otherwise, it checks to see if the
// first op of the call is a constant bitcast op that can result from LLVM not
// knowing the function declaration ahead of time. If it detects this
// situation, it will grab the function that is being cast and return that.
template <typename Callable>
Function* CallInstHandler::untangleCall(Callable& ci)
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

void CallInstHandler::handle(llvm::Instruction& inst)
{
    CallInst& call_inst = *cast<CallInst>(&inst);

    LLVMTypes types(call_inst.getContext());
    vector<Value*> args;

    Function* raw_called_func = call_inst.getCalledFunction();

    // don't do anything for LLVM instrinsic functions since we know we'll never instrument those functions
    if(raw_called_func && raw_called_func->isIntrinsic())
    {
        LOG_DEBUG() << "ignoring call to LLVM intrinsic function: " << raw_called_func->getName() << "\n";
        return;
    }

    Function* called_func = untangleCall(call_inst);

    if(called_func) // not a func ptr
        LOG_DEBUG() << "got a call to function " << called_func->getName() << "\n";
    else
        LOG_DEBUG() << "got a call to function via function pointer: " << call_inst << "\n";

    // Function that pushes an llvm int into args.
    function<void(const Type*, unsigned int)> push_int = bind(&vector<Value*>::push_back,
        ref(args), bind<Constant*>(&ConstantInt::get, _1, _2, false));

    // insert call to prepareCall to setup the structures needed to pass arg and return info efficiently
//    InstrumentedCall<Callable>* instrumented_call;
//    instrumentationCalls.push_back(instrumented_call = new InstrumentedCall<Callable>(ci, bb_call_idx));

    // Used to serialize all the inserts.
    CallInst* last_func = &call_inst;

    // if this call returns a value, we need to call addReturnValueLink
    ReturnsRealValue ret_real_val;
    if(ret_real_val(call_inst)) 
    {
        args.clear();
        push_int(types.i32(), ts_placer.getId(call_inst)); // dest ID
        CallInst* ret_val_link = CallInst::Create(ret_val_link_func, args.begin(), args.end(), "");
        ts_placer.add(*ret_val_link, *last_func);
        last_func = ret_val_link;
    }

    // link all the args that aren't passed by ref
    CallSite cs = CallSite::get(&call_inst);

    for(CallSite::arg_iterator arg_it = cs.arg_begin(), arg_end = cs.arg_end(); arg_it != arg_end; ++arg_it) 
    {
        Value& arg = **arg_it;
        LOG_DEBUG() << "checking arg: " << arg << "\n";

        if(!isa<PointerType>(arg.getType())) 
        {
            args.clear();
            CallInst* to_add;
            if(!isa<Constant>(&arg)) // not a constant so we need to call linkArgToAddr
            {
                push_int(types.i32(), ts_placer.getId(arg)); // Source ID.
                to_add = CallInst::Create(link_arg_func, args.begin(), args.end(), "");

                ts_placer.requestTimestamp(arg, *to_add);
            }
            else // this is a constant so call linkArgToConst instead (which takes no args)
                to_add = CallInst::Create(link_arg_const_func, args.begin(), args.end(), "");
            ts_placer.add(*to_add, *last_func);
            last_func = to_add;
        }
    } // end for(arg_it)

    // Insert prepare call.
    args.clear();
    uint32_t callsite_id = call_idx++;
    push_int(types.i64(), callsite_id);
    push_int(types.i64(), 0);              // Caller region ID. TODO: implement
    CallInst& prepare_call = 
        *CallInst::Create(prepare_call_func, args.begin(), args.end(), "");
    ts_placer.add(prepare_call, *last_func);
}

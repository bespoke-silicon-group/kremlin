#include <llvm/Metadata.h>
#include <llvm/Function.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/GlobalVariable.h>
#include <sstream>
#include <iomanip>

#include "InstrumentedCall.h"
#include "PassLog.h"
#include "LLVMTypes.h"
#include "ids/CallSiteIdGenerator.h"

using namespace llvm;

template <typename Callable>
InstrumentedCall<Callable>::InstrumentedCall(Callable* ci, uint64_t bb_call_idx) :
    InstrumentationCall(NULL, NULL, INSERT_BEFORE, NULL),
    ci(ci),
    id(CallSiteIdGenerator::generate(ci, bb_call_idx))
{
}

template <typename Callable>
InstrumentedCall<Callable>::~InstrumentedCall()
{
}

template <typename Callable>
unsigned long long InstrumentedCall<Callable>::getId() const
{
    return id;
}

/**
 * This function tries to untangle some strangely formed function calls.
 * If the call inst is a normal call inst then it just returns the function that is returned by
 * ci->getCalledFunction().
 * Otherwise, it checks to see if the first op of the call is a constant bitcast op that can
 * result from LLVM not knowing the function declaration ahead of time. If it detects this
 * situation, it will grab the function that is being cast and return that.
 */
template <typename Callable>
Function* InstrumentedCall<Callable>::untangleCall(Callable* ci)
{
    if(ci->getCalledFunction()) { return ci->getCalledFunction(); }

    Value* op0 = ci->getCalledValue(); // TODO: rename this to called_val
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


//template <typename Callable>
//void InstrumentedCall::instrument(std::map<Value*,unsigned int>& inst_to_id, InstrumentationCalls& front) 
//{
//    LLVMTypes types(ci->getContext());
//    std::vector<Value*> args;
//
//    // don't do anything for LLVM instrinsic functions since we know we'll never instrument those functions
//    if(!(ci->getCalledFunction() && ci->getCalledFunction()->isIntrinsic())) {
//        Function* called_func = untangleCall(ci);
//
//        if(called_func) // not a func ptr
//            LOG_DEBUG() << "got a call to function " << called_func->getName() << "\n";
//        else
//            LOG_DEBUG() << "got a call to function via function pointer: " << *ci;
//
//        // insert call to prepareCall to setup the structures needed to pass arg and return info efficiently
//        boost::uuids::random_generator gen;
//        uint64_t callsite_id = UuidToIntAdapter<uint64_t>::get(gen());
//
//        /*
//        std::string filename;
//        int filename;
//
//        std::ostringstream os;
//        os  << callsite_id << delim
//            << "callsite" << delim
//            << filename << delim
//            << line << delim
//            << line << delim;
//
//        new GlobalVariable(m, types.i8(), false, GlobalValue::ExternalLinkage, ConstantInt::get(types.i8(), 0), Twine(os.str()));
//        */
//        args.push_back(ConstantInt::get(types.i64(),callsite_id));  // Call site ID
//        args.push_back(ConstantInt::get(types.i64(),0));            // ID of region being called. TODO: Implement
//        front.addCallInst(ci, "prepareCall", args);
//        args.clear();
//
//        // if this call returns a value, we need to call addReturnValueLink
//        if(returnsRealValue(ci)) {
//            args.push_back(ConstantInt::get(types.i32(),inst_to_id[ci])); // dest ID
//            front.addCallInst(ci, "addReturnValueLink", args);
//            args.clear();
//        }
//
//        // link all the args that aren't passed by ref
//        CallSite cs = CallSite::get(ci);
//
//        for(CallSite::arg_iterator arg_it = cs.arg_begin(), arg_end = cs.arg_end(); arg_it != arg_end; ++arg_it) {
//            Value* the_arg = *arg_it;
//            LOG_DEBUG() << "checking arg: " << *the_arg << "\n";
//
//            if(!isa<PointerType>(the_arg->getType())) {
//                if(!isa<Constant>(*arg_it)) { // not a constant so we need to call linkArgToAddr
//                    args.push_back(ConstantInt::get(types.i32(),inst_to_id[the_arg])); // src ID
//
//                    front.addCallInst(ci, "linkArgToLocal", args);
//
//                    args.clear();
//                }
//                else { // this is a constant so call linkArgToConst instead (which takes no args)
//                    front.addCallInst(ci, "linkArgToConst", args);
//                }
//            }
//        } // end for(arg_it)
//    }
//    else {
//        LOG_DEBUG() << "ignoring call to LLVM intrinsic function: " << ci->getCalledFunction()->getName() << "\n";
//    }
//}

template <typename Callable>
void InstrumentedCall<Callable>::instrument()
{
    Module* m = ci->getParent()->getParent()->getParent();
    LLVMTypes types(m->getContext());

    // Add a global variable to encode the callsite
    std::string encoded_name;
    new GlobalVariable(*m, types.i8(), false, GlobalValue::ExternalLinkage, ConstantInt::get(types.i8(), 0), Twine(formatToString(encoded_name)));
}

template <typename Callable>
std::string& InstrumentedCall<Callable>::formatToString(std::string& buf) const
{
    std::ostringstream os;
    std::string fileName = "??";
    std::string funcName = "??";
    long long line = -1;

    if(MDNode *n = ci->getMetadata("dbg"))      // grab debug metadata from inst
    {
        DILocation loc(n);                      // get location info from metadata

        fileName = loc.getFilename();
        line = loc.getLineNumber();
        
        LOG_DEBUG() << "function: " << n->getFunction() << "\n";
    }

    os.fill('0');

	os  << PREFIX
	    << std::setw(16) << std::hex << getId() << std::dec << DELIMITER
		<< "callsiteId" << DELIMITER
		<< fileName << DELIMITER
		<< funcName << DELIMITER
		<< line << DELIMITER
		<< line << DELIMITER;

	buf = os.str();

	return buf;
}

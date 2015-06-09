#include <llvm/IR/Metadata.h>
#include <llvm/IR/Function.h>
#include "llvm/IR/DebugInfo.h"
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <sstream>
#include <iomanip>

#include "InstrumentedCall.h"
#include "PassLog.h"
#include "LLVMTypes.h"
#include "ids/CallSiteIdGenerator.h"

using namespace llvm;

/**
 * Constructs a new instrumented call.
 */
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

/**
 * @return The id.
 */
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

/**
 * Adds a global variable with the encoded debug information.
 */
template <typename Callable>
void InstrumentedCall<Callable>::instrument()
{
    Module* m = ci->getParent()->getParent()->getParent();
    LLVMTypes types(m->getContext());

    // Add a global variable to encode the callsite
    std::string encoded_name;
    new GlobalVariable(*m, types.i8(), false, GlobalValue::ExternalLinkage, ConstantInt::get(types.i8(), 0), Twine(formatToString(encoded_name)));
}

/**
 * Formats the call to a string.
 *
 * @param buf The buffer to place the formatted data.
 * @return The passed buffer.
 */
template <typename Callable>
std::string& InstrumentedCall<Callable>::formatToString(std::string& buf) const
{
	LOG_DEBUG() << "Formatting to string: " << *ci << "\n";
    std::ostringstream os;
    std::string fileName = "??";
    std::string funcName = "??";
    long long line = -1;

    if(MDNode *n = ci->getMetadata("dbg"))      // grab debug metadata from inst
    {
        DILocation loc(n);                      // get location info from metadata

		std::string rawName = loc.getFilename();
		size_t substr_start = rawName.rfind('/');
		if (substr_start == std::string::npos) {
			substr_start = 0;
		}
		else {
			substr_start++;
		}
		fileName = rawName.substr(substr_start);
		funcName = ci->getParent()->getParent()->getName();
		if (funcName.at(0) == '~') funcName.replace(0, 1, "destructor_");

        line = loc.getLineNumber();
        
        //LOG_DEBUG() << "function: " << ci->getParent()->getParent() << "\n";
    }
	else {
		LOG_DEBUG() << "Couldn't find debug info!\n";
	}

	assert(fileName.compare("??") != 0 && funcName.compare("??") != 0);

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

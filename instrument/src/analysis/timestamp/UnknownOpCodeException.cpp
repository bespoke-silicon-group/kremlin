#include "UnknownOpCodeException.h"

#include <string>
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

/**
 * Constructs a new exception.
 *
 * @param inst The instruction with the unknown op code.
 */
UnknownOpCodeException::
UnknownOpCodeException(const llvm::Instruction* inst) throw() :
    inst(inst)
{
    raw_string_ostream os(msg);
    os << "Unknown op code (" << inst->getOpcode() << ") for " << *inst;
    os.str();
}

UnknownOpCodeException::
~UnknownOpCodeException() throw()
{
}

/**
 * Returns the error message.
 */
const char* UnknownOpCodeException::
what() const throw()
{
    return msg.c_str();
}

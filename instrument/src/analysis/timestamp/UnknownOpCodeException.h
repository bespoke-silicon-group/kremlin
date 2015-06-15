#ifndef UNKNOWN_OP_CODE_EXCEPTION_H
#define UNKNOWN_OP_CODE_EXCEPTION_H

#include <exception>
#include <llvm/IR/Instruction.h>

/**
 * Thrown for unknown op codes. The complete list of op-codes are here: <llvm/Instruction.def>
 */
class UnknownOpCodeException : public std::exception
{
    const llvm::Instruction* inst;
    std::string msg;

    public:
    UnknownOpCodeException(const llvm::Instruction* inst) throw();
    virtual ~UnknownOpCodeException() throw();

    virtual const char* what() const throw();
};

#endif // UNKNOWN_OP_CODE_EXCEPTION_H

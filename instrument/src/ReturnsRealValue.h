#ifndef RETURNS_REAL_VALUE_H
#define RETURNS_REAL_VALUE_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>

/**
 * Checks whether this returns a non-ptr value (not void)
 */
class ReturnsRealValue
{
    public:
    bool operator()(const llvm::Type& ret_type);
    bool operator()(llvm::CallInst& ci);
    bool operator()(llvm::Function& func);
    bool operator()(llvm::InvokeInst& ii);
};

#endif // RETURNS_REAL_VALUE_H

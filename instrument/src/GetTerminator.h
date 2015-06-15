#ifndef GET_TERMINATOR_H
#define GET_TERMINATOR_H

#include "UnaryFunctionConst.h"
#include <llvm/IR/BasicBlock.h>

struct GetTerminator : public UnaryFunctionConst<llvm::BasicBlock*, llvm::Instruction*> 
{
    virtual llvm::Instruction* operator()(llvm::BasicBlock* bb) const;
    virtual ~GetTerminator() {}
};

#endif // GET_TERMINATOR_H

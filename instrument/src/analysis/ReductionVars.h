#ifndef REDUCTION_VARS_H
#define REDUCTION_VARS_H

#include <llvm/Pass.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Analysis/LoopInfo.h>

#include <set>

#include "PassLog.h"

// TODO: Make a consistent naming scheme between this and induction variables.

class ReductionVars : public llvm::FunctionPass
{
    public:
    static char ID;

    ReductionVars();
    virtual ~ReductionVars();

    virtual bool runOnFunction(llvm::Function &F);
    bool isReductionVar(llvm::Instruction* inst) const;

    virtual void getAnalysisUsage(llvm::AnalysisUsage &au) const;

    private:
    PassLog& log;
    std::set<llvm::Instruction*> red_var_ops;
   
    std::vector<llvm::Instruction*> getNonPhiUsesInLoop(llvm::LoopInfo& LI, llvm::Loop* loop, llvm::Value* val);
    void getReductionVars(llvm::LoopInfo& LI, llvm::Loop* loop);

    llvm::Instruction* getReductionVarOp(llvm::LoopInfo& LI, llvm::Loop* loop, llvm::Value *val);

    llvm::Instruction* getArrayReductionVarOp(llvm::LoopInfo& LI, llvm::Loop* loop, llvm::Value *val);

    void getArrayReductionVars(llvm::LoopInfo& LI, llvm::Loop* loop, std::set<llvm::Instruction*>& red_var_ops);
    bool isReductionOpType(llvm::Instruction* inst);
};

#endif // REDUCTION_VARS_H

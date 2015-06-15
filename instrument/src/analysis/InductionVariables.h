#ifndef INDUCTION_VARIABLES_H
#define INDUCTION_VARIABLES_H

#include <set>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>
#include "PassLog.h"

class InductionVariables
{
    public:
    InductionVariables(llvm::LoopInfo& li);
    virtual ~InductionVariables() {}

    bool isInductionVariable(llvm::PHINode& inst) const;
    bool isInductionIncrement(llvm::Instruction& inst) const;

    private:
    typedef std::set<llvm::PHINode*> Variables;
    typedef std::set<llvm::Instruction*> Increments;

    // Gets the induction var increment.
    llvm::Instruction* getCanonicalInductionVariableIncrement(llvm::PHINode* ind_var, llvm::Loop* loop) const;

    // adds all the canon ind. var increments for a loop (including its
	// subloops) to ind_var_increment_ops set
    void gatherInductionVarIncrements(llvm::Loop* loop, Variables& ind_vars, Increments& ind_var_increment_ops);

    Variables _inductionVars;
    Increments _inductionVarIncrementOps;

	PassLog& log;
};

#endif // INDUCTION_VARIABLES_H

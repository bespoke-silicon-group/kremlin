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
    typedef std::set<llvm::Instruction*> Updates;

    // Gets the induction var update (should be either add or sub instruction)
    llvm::Instruction* getInductionVariableUpdate(llvm::PHINode* ind_var, llvm::Loop* loop) const;

	// Adds all the ind. var updates for a loop (including its subloops) to
	// ind_var_update_ops set
    void gatherInductionVarUpdates(llvm::Loop* loop, Variables& ind_vars, Updates& ind_var_increment_ops);

    Variables induction_vars;
    Updates ind_var_update_ops;

	PassLog& log;
};

#endif // INDUCTION_VARIABLES_H

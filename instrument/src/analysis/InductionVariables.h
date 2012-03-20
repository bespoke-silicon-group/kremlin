#ifndef INDUCTION_VARIABLES_H
#define INDUCTION_VARIABLES_H

#include <set>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Instructions.h>

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

    // adds all the canon ind. var increments for a loop (including its subloops) to canon_incs set
    void addCIVIToSet(llvm::Loop* loop, std::set<llvm::PHINode*>& canon_indvs, std::set<llvm::Instruction*>& canon_incs);

    Variables variables;
    Increments increments;
};

#endif // INDUCTION_VARIABLES_H

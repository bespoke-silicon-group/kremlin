#include "InductionVariables.h"

using namespace llvm;

InductionVariables::InductionVariables(llvm::LoopInfo& li) :
	log(PassLog::get())
{
    for(LoopInfo::iterator loop = li.begin(), loop_end = li.end(); loop != loop_end; ++loop)
        gatherInductionVarIncrements(*loop, _inductionVars, _inductionVarIncrementOps);
}

bool InductionVariables::isInductionVariable(llvm::PHINode& inst) const 
{
    return _inductionVars.find(&inst) != _inductionVars.end();
}

bool InductionVariables::isInductionIncrement(llvm::Instruction& inst) const
{
    return _inductionVarIncrementOps.find(&inst) != _inductionVarIncrementOps.end();
}

// Gets the induction var increment. This used to be part of LLVM but was removed for reasons
// unbeknownst to me :(
Instruction* InductionVariables::getCanonicalInductionVariableIncrement(PHINode* ind_var, Loop* loop) const 
{
    bool P1InLoop = loop->contains(ind_var->getIncomingBlock(1));
    return cast<Instruction>(ind_var->getIncomingValue(P1InLoop));
}

// adds all the canon ind. var increments for a loop (including its subloops)
// to ind_var_increment_ops set
void InductionVariables::gatherInductionVarIncrements(Loop* loop, Variables& ind_vars, Increments& ind_var_increment_ops) {

    std::vector<Loop*> sub_loops = loop->getSubLoops();

    // check all subloops for canon var increments
    if(sub_loops.size() > 0) {
        for(std::vector<Loop*>::iterator sub_loop = sub_loops.begin(), sl_end = sub_loops.end(); sub_loop != sl_end; ++sub_loop) 
            gatherInductionVarIncrements(*sub_loop,ind_vars,ind_var_increment_ops);
    }

    PHINode* induction_var = loop->getCanonicalInductionVariable();

    // couldn't find an ind var so there is nothing left to do for this loop
    if(induction_var == NULL) { return; }

    Instruction* induction_var_increment_op = getCanonicalInductionVariableIncrement(induction_var,loop);

    LOG_DEBUG() << "Found increment of induction variable: " 
	<< induction_var_increment_op->getName() << "\n";
    ind_vars.insert(induction_var);
    ind_var_increment_ops.insert(induction_var_increment_op);
}


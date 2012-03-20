#include "InductionVariables.h"

using namespace llvm;

InductionVariables::InductionVariables(llvm::LoopInfo& li)
{
    for(LoopInfo::iterator loop = li.begin(), loop_end = li.end(); loop != loop_end; ++loop)
        addCIVIToSet(*loop, variables, increments);
}

bool InductionVariables::isInductionVariable(llvm::PHINode& inst) const 
{
    return variables.find(&inst) != variables.end();
}

bool InductionVariables::isInductionIncrement(llvm::Instruction& inst) const
{
    return increments.find(&inst) != increments.end();
}

// Gets the induction var increment. This used to be part of LLVM but was removed for reasons
// unbeknownst to me :(
Instruction* InductionVariables::getCanonicalInductionVariableIncrement(PHINode* ind_var, Loop* loop) const 
{
    bool P1InLoop = loop->contains(ind_var->getIncomingBlock(1));
    return cast<Instruction>(ind_var->getIncomingValue(P1InLoop));
}

// adds all the canon ind. var increments for a loop (including its subloops) to canon_incs set
void InductionVariables::addCIVIToSet(Loop* loop, std::set<PHINode*>& canon_indvs, std::set<Instruction*>& canon_incs) {

    std::vector<Loop*> sub_loops = loop->getSubLoops();

    // check all subloops for canon var increments
    if(sub_loops.size() > 0) {
        for(std::vector<Loop*>::iterator sl_it = sub_loops.begin(), sl_end = sub_loops.end(); sl_it != sl_end; ++sl_it) 
            addCIVIToSet(*sl_it,canon_indvs,canon_incs);
    }

    PHINode* civ = loop->getCanonicalInductionVariable();

    // couldn't find an ind var so there is nothing left to do for this loop
    if(civ == NULL) { return; }

    Instruction* civ_inc = getCanonicalInductionVariableIncrement(civ,loop);

    //LOG_DEBUG() << "found canon ind var increment: " << civi->getName() << "\n";
    canon_indvs.insert(civ);
    canon_incs.insert(civ_inc);
}


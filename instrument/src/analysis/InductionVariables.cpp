#include <llvm/IR/Constants.h>

#include "InductionVariables.h"

using namespace llvm;

InductionVariables::InductionVariables(llvm::LoopInfo& li) :
	log(PassLog::get())
{
    for(LoopInfo::iterator loop = li.begin(), loop_end = li.end(); loop != loop_end; ++loop)
        gatherInductionVarUpdates(*loop, induction_vars, ind_var_update_ops);
}

bool InductionVariables::isInductionVariable(llvm::PHINode& inst) const 
{
    return induction_vars.find(&inst) != induction_vars.end();
}

bool InductionVariables::isInductionIncrement(llvm::Instruction& inst) const
{
    return ind_var_update_ops.find(&inst) != ind_var_update_ops.end();
}

/**
 * Check to see if a loop has an induction variable.
 *
 * @note This code is modified from the LLVM Loop class' getCanonicalInductionVar
 * 	member function. Since we don't care about induction variables being
 * 	canonical, they can start at any value (not just 0) and either increment
 * 	or decrement by a constant amount (not just increment by 1).
 *
 * @param loop The loop to check for induction variable.
 * @return PHInode for the induction variable if one exists, null otherwise
 */
static PHINode *getInductionVariable(Loop *loop)
{
	BasicBlock *H = loop->getHeader();

	BasicBlock *Incoming = nullptr, *Backedge = nullptr;
	pred_iterator PI = pred_begin(H);
	assert(PI != pred_end(H) &&
			"Loop must have at least one backedge!");
	Backedge = *PI++;
	if (PI == pred_end(H)) return nullptr;  // dead loop
	Incoming = *PI++;
	if (PI != pred_end(H)) return nullptr;  // multiple backedges?

	if (loop->contains(Incoming)) {
		if (loop->contains(Backedge))
			return nullptr;
		std::swap(Incoming, Backedge);
	} else if (!loop->contains(Backedge))
		return nullptr;

	// Loop over all of the PHI nodes, looking for a indvar.
	for (BasicBlock::iterator I = H->begin(); isa<PHINode>(I); ++I) {
		PHINode *PN = cast<PHINode>(I);
		if (isa<ConstantInt>(PN->getIncomingValueForBlock(Incoming)))
			if (Instruction *Inc =
					dyn_cast<Instruction>(PN->getIncomingValueForBlock(Backedge)))
				if ((Inc->getOpcode() == Instruction::Add 
							|| Inc->getOpcode() == Instruction::Sub) 
						&& Inc->getOperand(0) == PN)
					if (isa<ConstantInt>(Inc->getOperand(1)))
						return PN;
	}
	return nullptr;
}

// Gets the induction var increment. This used to be part of LLVM but was removed for reasons
// unbeknownst to me :(
Instruction* InductionVariables::getInductionVariableUpdate(PHINode* ind_var, Loop* loop) const 
{
    bool P1InLoop = loop->contains(ind_var->getIncomingBlock(1));
    return cast<Instruction>(ind_var->getIncomingValue(P1InLoop));
}

// adds all the canon ind. var increments for a loop (including its subloops)
// to ind_var_increment_ops set
void InductionVariables::gatherInductionVarUpdates(Loop* loop, Variables& ind_vars, Updates& ind_var_increment_ops) 
{

	std::vector<Loop*> sub_loops = loop->getSubLoops();

	// check all subloops for canon var increments
	if(sub_loops.size() > 0) {
		for(std::vector<Loop*>::iterator sub_loop = sub_loops.begin(), sl_end = sub_loops.end(); sub_loop != sl_end; ++sub_loop) 
			gatherInductionVarUpdates(*sub_loop,ind_vars,ind_var_increment_ops);
	}

	PHINode* induction_var = getInductionVariable(loop);

	// couldn't find an ind var so there is nothing left to do for this loop
	if(induction_var) {
		Instruction* induction_var_increment_op = getInductionVariableUpdate(induction_var,loop);

		LOG_DEBUG() << "Found update of induction variable: " 
			<< induction_var_increment_op->getName() << "\n";
		ind_vars.insert(induction_var);
		ind_var_increment_ops.insert(induction_var_increment_op);
	}
}


#include <foreach.h>

#include "analysis/ControlDependence.h"

using namespace llvm;

/**
 * Function object that returns if a block is a successor of the
 * target block.
 */
class IsSuccessor : public std::unary_function<BasicBlock*, bool> 
{
    public:
    /**
     * Initializes a new comparator.
     *
     * @param target The block to check for successors of.
     */
    IsSuccessor(BasicBlock* target) : target(target) 
    {
        addSuccessors(target);
    }

    /**
     * Returns true if the block is a successor of the block passed to
     * the constructor.
     *
     * @param block The block to check.
     * @return true if the block is a successor.
     */
    bool operator()(BasicBlock* block) {
        return successors.find(block) != successors.end();
    }

    private:
    BasicBlock* target;                   //The block to look for successors of.
    std::set<BasicBlock*> successors;     //The successors of the block.

    /**
     * Adds successors of the target block.
     *
     * @param target The block to get successors of.
     */
    void addSuccessors(BasicBlock* target) {
        TerminatorInst* terminator = target->getTerminator();

        if(isa<BranchInst>(terminator) || isa<SwitchInst>(terminator)) {

            // check all successors to target
            for(unsigned int i = 0; i < terminator->getNumSuccessors(); i++) {
                BasicBlock* successor = terminator->getSuccessor(i);

                //If this was inserted not already
                if(!(*this)(successor)) {
                    successors.insert(successor);
                    addSuccessors(successor);
                }
            }
        }
    }
};

/**
 * Constructs a new control dependence analysis.
 */
ControlDependence::ControlDependence(Function& func, llvm::DominatorTree& dt, PostDominanceFrontier& pdf) :
    dt(dt),
    log(PassLog::get()),
    pdf(pdf)
{
    assert(!func.isDeclaration());

    createIDomMap(func);

    foreach(BasicBlock& bb, func)
        foreach(Instruction& inst, bb)
        {
            PHINode* phi = dyn_cast<PHINode>(&inst);
            if(phi)
            {
                std::auto_ptr<ControllingBlocks> blocks(new ControllingBlocks());
                getPhiControllingBlocks(*phi, *blocks.get());
                phi_to_controllers.insert(phi, blocks);
            }
        }
}

/**
 * Preconstructs the immediate dominator map.
 */
void ControlDependence::createIDomMap(Function& func) 
{
    foreach(BasicBlock& bb, func)
    {
        LOG_DEBUG() << "Getting immediate dominator of " << bb << "\n";
        idom[&bb] = getImmediateDominator(&bb);
    }
}

/**
 * @return The immediate dominator of the basic block.
 */
BasicBlock* ControlDependence::getImmediateDominator(BasicBlock* blk) 
{
    DomTreeNode* node = dt.getNode(blk);
    DomTreeNodeBase<BasicBlock>* idom = node ? node->getIDom() : NULL;
    return idom ? idom->getBlock() : NULL;
}

/**
 * Returns the blocks that control the incoming blocks of the phi
 * instruction.
 *
 * @param phi     The phi node to get the incoming block's conditions.
 * @param result  A place to put the results.
 * @return        The argument passed as the result.
 */
ControlDependence::ControllingBlocks& 
ControlDependence::getPhiControllingBlocks(PHINode& phi, ControllingBlocks& result)
{
    result.clear();

    BasicBlock* bb_containing_phi = phi.getParent();

    std::map<Value*, std::set<BasicBlock*> > equivelent_values;

    // Create map between incoming values and the block(s) they come from.
    unsigned int num_incoming = phi.getNumIncomingValues();
    for(unsigned int i = 0; i < num_incoming; i++) {
        Value* incoming_value = phi.getIncomingValue(i);
        equivelent_values[incoming_value].insert(phi.getIncomingBlock(i));
    }

    // For each incoming value, we find the nearest common dominator of all incoming
    // blocks that contain that value, adding it to the incoming_blocks set.
    std::set<BasicBlock*> incoming_blocks;
    for(std::map<Value*, std::set<BasicBlock*> >::iterator it = equivelent_values.begin(), end = equivelent_values.end(); it != end; it++)
        incoming_blocks.insert(findNearestCommonDominator(it->second));

    // Add all the controlling blocks of the incoming blocks.
    foreach(BasicBlock* bb, incoming_blocks)
        getPhiControllingBlocks(bb, idom[bb_containing_phi], dt, result);

    return result;
}

/**
 * Recursivly gets for the controlling blocks of the controlling
 * blocks.
 *
 * @param target    The block to get the controlling block of.
 * @param limit     The block to stop at. The result may contain
 *                  limit, but it will not contain anything that 
 *                  limit does not dominate.
 * @param dt        The dominance tree for this function.
 * @param result    The location to put the controlling blocks.
 * @return          The value passed as result.
 */
ControlDependence::ControllingBlocks& 
ControlDependence::getPhiControllingBlocks(BasicBlock* target, BasicBlock* limit, DominatorTree& dt, ControllingBlocks& result)
{
    if(!limit || !dt.dominates(limit, target))
        return result;

    IsSuccessor is_successor(target);

    //Add the controlling block
    BasicBlock* controlling_block = getControllingBlock(target, is_successor(target));

    //Only add the controlling block if we haven't seen it before and it's not null.
    if(controlling_block && result.find(controlling_block) == result.end()) 
    {
        result.insert(controlling_block);

        //Add the controlling block of the found controlling block.
        return getPhiControllingBlocks(controlling_block, limit, dt, result);
    }
    return result;
}

/**
 * @return The common dominator of all of the blocks in the set.
 */
BasicBlock* ControlDependence::findNearestCommonDominator(const std::set<BasicBlock*>& blocks) 
{
    BasicBlock* commonDominator = *blocks.begin();

    for(std::set<BasicBlock*>::iterator it = blocks.begin(), end = blocks.end(); it != end; it++)
    {
        commonDominator = dt.findNearestCommonDominator(commonDominator, *it);
        assert(commonDominator);
    }

    return commonDominator;
}

/**
 * Returns the closest block in the CFG that has a control condition
 * that could lead away from executing this block.
 *
 * This actually looks for the node in the post-domininance frontier
 * that isn't itself.
 *
 * @param blk The block to find the control condition.
 * @return the closest controlling block.
 */
BasicBlock* ControlDependence::getControllingBlock(BasicBlock* blk)
{
    return getControllingBlock(blk, true);
}

/**
 * Returns the controlling block of blk or NULL if there is none.
 *
 * @param blk           The block to get the condition of.
 * @param consider_self True if blk should be considered.
 * @return the controlling block of blk or NULL if there is none.
 */
BasicBlock* ControlDependence::getControllingBlock(BasicBlock* blk, bool consider_self)
{
    LOG_DEBUG() << "Controlling block search of " << blk->getName() << "\n";

    BasicBlock* ret_val = NULL;

    //LOG_DEBUG() << blk->getName() << "\n";


    // get post dominance frontier for blk
    DominanceFrontierBase<BasicBlock>::iterator dsmt_it = pdf.find(blk);

    // sanity check to make sure an entry in the pdf exists for this blk
    if(dsmt_it == pdf.end())
    {
        LOG_ERROR() << "Could not find blk " << blk->getName() << 
            " in post dominance frontier of function " << blk->getParent()->getName() <<
            "! Contents of the pdf:"<< "\n";

        for(PostDominanceFrontier::iterator it = pdf.begin(), end = pdf.end(); it != end; it++)
        {
            if(it->first)
                LOG_DEBUG() << it->first->getName();
            else
                LOG_DEBUG() << "null? [" << it->first << "]";

            LOG_DEBUG() << " {";
            for(std::set<BasicBlock*>::iterator it2 = it->second.begin(), it2_end = it->second.end(); 
                it2 != it2_end; 
                it2++)
            {
                LOG_DEBUG() << (*it2)->getName() << ", ";
            }
            LOG_DEBUG() << "}" << "\n";
        }
        return NULL;
    }

    // Look at all blocks in post-dom frontier and find one that dominates the current block
    for(DominanceFrontierBase<BasicBlock>::DomSetType::iterator dst_it = dsmt_it->second.begin(), dst_end = dsmt_it->second.end(); dst_it != dst_end; ++dst_it) {
        BasicBlock* candidate = *dst_it;

        LOG_DEBUG() << "looking at " << candidate->getName() << "\n";

        if((consider_self || candidate != blk) && dt.dominates(candidate,blk)) { // not itself so we found the controlling blk
            //LOG_DEBUG() << "found controlling blk: " << candidate->getName() << "\n";
            ret_val = candidate;
            break;
        }
    }

	// @TRICKY: An invoke instruction doesn't really form a control dependence
	// in the sense that it contains a condition that will determine which
	// path we will take. Instead, the decision is based on whether the
	// exception is raised or not.
	if (ret_val != NULL && dyn_cast<InvokeInst>(ret_val->getTerminator()) != NULL) {
		LOG_DEBUG() << "Ignoring \"controlling\" block that ends with invoke\n";
		return NULL;
	}

    return ret_val;
}

/**
 * @return The control condition contained in the basic block. All of the
 * successors should be control dependent on the found value.
 */
llvm::Value* ControlDependence::getControllingCondition(llvm::BasicBlock* bb)
{
    TerminatorInst* term = bb->getTerminator();
	LOG_DEBUG() << *term << "\n";
    BranchInst* br_inst;
    if((br_inst = dyn_cast<BranchInst>(term)) && br_inst->isConditional())
        return br_inst->getCondition();

    else if(SwitchInst* sw_inst = dyn_cast<SwitchInst>(term))
        return sw_inst->getCondition();

    LOG_DEBUG() << "No controlling val found for bb: " << bb->getName() << "\n";

    return NULL;
}

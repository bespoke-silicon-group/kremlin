#include <boost/lexical_cast.hpp>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include "PhiHandler.h"
#include "LLVMTypes.h"

#define MAX_SPECIALIZED 5

using namespace llvm;
using namespace boost;
using namespace std;

/**
 * Function object that returns if a block is a predecessor of the
 * target block.
 */
class IsPredecessor : public std::unary_function<BasicBlock*, bool> {
    BasicBlock* target;                   //The block to look for successors of.
    std::set<BasicBlock*> predecessors;   //The predecessors of the block.

    public:

    /**
     * Initializes a new comparator.
     *
     * @param target The block to check for successors of.
     */
    IsPredecessor(BasicBlock* target) : target(target) {
        std::set<BasicBlock*> searched;
        getClosestPredecessors(target, &target->getParent()->getEntryBlock(), predecessors, searched);
        //addPredecessors(&target->getParent()->getEntryBlock(), target);
    }

    /**
     * Returns true if the block is a successor of the block passed to
     * the constructor.
     *
     * @param block The block to check.
     * @return true if the block is a successor.
     */
    bool operator()(BasicBlock* block) {
        return predecessors.find(block) != predecessors.end();
    }

    /**
     * Finds the closest predecessors of the target basic block.
     *
     * @param target      The basic to look for predecessors of.
     * @param predecessor The basic block to consider. Begin this with a basic
     *                    block that dominates the target.
     * @param results     A set of all the predecessors.
     * @param searched    the basic blocks already searched through.
     */
    static std::set<BasicBlock*>& getClosestPredecessors(
        BasicBlock* target, BasicBlock* predecessor, std::set<BasicBlock*>& results, std::set<BasicBlock*>& searched) 
    {
        // don't go any further if we already search this predecessor
        if(searched.find(predecessor) != searched.end())
            return results;

        // mark that we have search this one
        searched.insert(predecessor);

        TerminatorInst* terminator = predecessor->getTerminator();

        if(isa<BranchInst>(terminator) || isa<SwitchInst>(terminator))
		{
            // check all successors to predecessor
            for(unsigned int i = 0; i < terminator->getNumSuccessors(); i++) 
            {
                BasicBlock* successor = terminator->getSuccessor(i);

                // if target is a successor, then we put this in results
                if(successor == target)
                    results.insert(predecessor);

                // recurse, with successor as the next pred to look at
                getClosestPredecessors(target, successor,  results, searched);

                // if successor is part of the results, then this pred should also be there
                if(results.find(successor) != results.end())
                    results.insert(predecessor);
            }
		}

        return results;
    }

};

/**
 * Constructs a new phi instruction handler.
 *
 * @param ts_placer The timestamp placer this handler is associated with.
 */
PhiHandler::PhiHandler(TimestampPlacer& ts_placer) :
    controlDependence(ts_placer.getAnalyses().cd),
    dominatorTree(ts_placer.getAnalyses().dt),
    inductionVars(ts_placer.getAnalyses().li),
    loopInfo(ts_placer.getAnalyses().li),
    log(PassLog::get()),
    reductionVars(ts_placer.getAnalyses().rv),
    timestampPlacer(ts_placer)
{
    // Set up the opcodes
    opcodes.push_back(Instruction::PHI);

    // Setup the phiLoggingFunc
    Module& m = *timestampPlacer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<Type*> args;
    args.push_back(types.i32());
    args.push_back(types.i32());
    args.push_back(types.i32());
	ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
    FunctionType* func_type = FunctionType::get(types.voidTy(), *aref, true);
	delete aref;
    phiLoggingFunc = cast<Function>(m.getOrInsertFunction("_KPhi", func_type));

    // Setup specialized funcs
    args.clear();
    args.push_back(types.i32());
    args.push_back(types.i32());
    for(size_t i = 1; i < MAX_SPECIALIZED; i++)
    {
        args.push_back(types.i32());
		aref = new ArrayRef<Type*>(args);
        FunctionType* type = FunctionType::get(types.voidTy(), *aref, false);
		delete aref;
        Function* func = cast<Function>(m.getOrInsertFunction(
                "_KPhi" + lexical_cast<string>(i) + "To1", type));
        specializedPhiLoggingFuncs.insert(make_pair(i, func));
    }

    // addCondFunc
    args.clear();
    args.push_back(types.i32());
    args.push_back(types.i32());
	aref = new ArrayRef<Type*>(args);
    FunctionType* add_cond_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;
    addCondFunc = cast<Function>(m.getOrInsertFunction(
            "_KPhiAddCond", add_cond_type));

    // inductionFunc
    args.clear();
    args.push_back(types.i32());
	aref = new ArrayRef<Type*>(args);
    FunctionType* induc_var_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;
    inductionFunc = cast<Function>(m.getOrInsertFunction(
            "_KInduction", induc_var_type));
}

const TimestampPlacerHandler::Opcodes& PhiHandler::getOpcodes()
{
    return opcodes;
}

/**
 * Handles induction variables.
 */
void PhiHandler::handleIndVar(PHINode& phi)
{
    // If this a PHI that is a loop induction var, we only call _KInduction when
    // we first enter the loop so that we don't get caught in a dependency
    // cycle. For example, the induction var increment would get the time of t_init
    // for the first iter, which would then force the next iter's t_init to be
    // one higher and so on and so forth...
    LOG_DEBUG() << "processing canonical induction var: " << phi << "\n";

	// Loop through all incoming vals of PHI and find the one that is constant
	// (i.e. initializes the loop to 0).  We then insert a call to
	// _KInduction() to permanently set the time of this to whatever
	// t_init happens to be in the BB where that value comes from.
    int index_of_const = -1;

    for(unsigned int idx = 0; idx < phi.getNumIncomingValues(); ++idx) {
        if(isa<ConstantInt>(phi.getIncomingValue(idx)) && index_of_const == -1) {
            index_of_const = idx;
        }
        else if(isa<ConstantInt>(phi.getIncomingValue(idx))) {
            LOG_ERROR() << "found multiple incoming constants to induction var phi node\n";
            log.close();
            assert(0);
        }
    }

    if(index_of_const == -1) {
        LOG_ERROR() << "could not find constant incoming value to induction variable phi node\n";
        log.close();
        assert(0);
    }

    BasicBlock& bb_assoc_with_const = *phi.getIncomingBlock(index_of_const);

    // finally, we will insert the call to _KInduction
    LLVMTypes types(phi.getContext());
    std::vector<Value*> log_func_args;

    log_func_args.push_back(ConstantInt::get(types.i32(), timestampPlacer.getId(phi), false));
	ArrayRef<Value*> *aref = new ArrayRef<Value*>(log_func_args);
    CallInst& ci = *CallInst::Create(inductionFunc, *aref, "");
	delete aref;
    timestampPlacer.constrainInstPlacement(ci, *bb_assoc_with_const.getTerminator());
}

/**
 * @return A phi node containing the virtual register table index of the
 * incoming value's timestamp.
 */
PHINode& PhiHandler::identifyIncomingValueId(PHINode& phi)
{
    LLVMTypes types(phi.getContext());
    PHINode& incoming_val_id_phi = *PHINode::Create(types.i32(), 0, "phi-incoming-val-id");

	// Construct a phi node corresponding to all non-const incoming value id's
	// in the input phi. We also make sure that the incoming val's timestamp
	// is available before the end of the incoming block associated with that
	// value.
    for(unsigned int i = 0; i < phi.getNumIncomingValues(); i++) 
    {

        BasicBlock& incoming_block = *phi.getIncomingBlock(i);
        Value& incoming_val = *phi.getIncomingValue(i);

        unsigned int incoming_id = 0; // default ID to 0 (i.e. a constant)

        // if incoming val isn't a constant, we grab its ID
        if(!isa<Constant>(&incoming_val))
        {
            incoming_id = timestampPlacer.getId(incoming_val);
            timestampPlacer.requireValTimestampBeforeUser(incoming_val, *incoming_block.getTerminator());
        }

        incoming_val_id_phi.addIncoming(ConstantInt::get(types.i32(),incoming_id), &incoming_block);
    }

    return incoming_val_id_phi;
}

/**
 * @param bb The block to find the control timestamp index.
 * @return the control condition's virtual register table timestamp index. 
 * This will be the condition in bb.
 */
unsigned int PhiHandler::getConditionIdOfBlock(llvm::BasicBlock& bb)
{
    Value& controlling_cond = *controlDependence.getControllingCondition(&bb);
    timestampPlacer.requireValTimestampBeforeUser(controlling_cond, *bb.getTerminator());
    return timestampPlacer.getId(controlling_cond);
}

/**
 * Returns all of the condtions the phi node depends on.
 *
 * @return A vector of phi nodes. Each of the phi nodes contains either the
 * virtual register table index of the condition's timestamp OR 0 if the
 * condition is not dominated for the given branch.
 */
 // TODO: code review... this is black magic right now
vector<PHINode*>& PhiHandler::getConditions(PHINode& phi, vector<PHINode*>& control_deps)
{
    LLVMTypes types(phi.getContext());
    BasicBlock& bb = *phi.getParent();

    // Get all the controllers of every block from the one containing the phi
    // to the block that dominates the phi.
    std::set<BasicBlock*> controllers;
    controlDependence.getPhiControllingBlocks(phi, controllers);

    foreach(BasicBlock* controller, controllers)
    {
        LOG_DEBUG() << "controller to " << bb.getName() << ": " << controller->getName() << "\n";
        
        PHINode* incoming_condition_addr = PHINode::Create(types.i32(), 0, "phi-incoming-condition");

        std::map<BasicBlock*, Value*> incoming_value_addrs;

        bool at_least_one_controller = false;

        // check all preds of current basic block
        for(unsigned int i = 0; i < phi.getNumIncomingValues(); i++) 
        {
            BasicBlock* incoming_block = phi.getIncomingBlock(i);
            unsigned int incoming_id = 0;

            // If this controller dominates incoming block, get the id of condition at end of pred
            if(dominatorTree.dominates(controller, incoming_block)) 
            {
                LOG_DEBUG() << controller->getName() << " dominates " << incoming_block->getName() << "\n";

                incoming_id = getConditionIdOfBlock(*controller);
                at_least_one_controller = true;
            } 

            incoming_condition_addr->addIncoming(ConstantInt::get(types.i32(),incoming_id), incoming_block);
        }
        
        if(at_least_one_controller)
            control_deps.push_back(incoming_condition_addr);
    }
    return control_deps;
}

/**
 * Add loop conditions that occur after the phi instruction.
 *
 * @param phi The phi node that is potentially in a loop.
 */
void PhiHandler::handleLoops(llvm::PHINode& phi)
{
    std::vector<Value*> args;
    LLVMTypes types(phi.getContext());
#if 0
    function<void(unsigned int)> push_int = bind(&vector<Value*>::push_back, 
        ref(args), bind<Constant*>(&ConstantInt::get, types.i32(), _1, false));
#endif

    BasicBlock& phi_bb = *phi.getParent();

    // Only handle loops.
    if(!loopInfo.isLoopHeader(&phi_bb)) return;

    // Only try if this branches loop header branches to multiple targets.
    BranchInst* phi_bb_terminating_branch = dyn_cast<BranchInst>(phi_bb.getTerminator());
    if(
		phi_bb_terminating_branch == NULL
		|| !phi_bb_terminating_branch->isConditional()
	  ) { return; }

    // No need to add conditions if it happens to be a constant. 
    // (e.g. while(true))
    Value& controlling_cond = *controlDependence.getControllingCondition(&phi_bb);
    if(isa<Constant>(&controlling_cond)) return;

    Loop* associated_loop = loopInfo.getLoopFor(&phi_bb);

    // Only looks like do loops if there are successors outside the
    // loop and the successor is not the same block.
	// FIXME: This looks hackish. Can't we do this in one loop instead
	// of 2???
    bool is_do_loop = true;

	// First, we'll look for a successor that isn't in the associated loop. If
	// we find it, we temporarily set is_do_loop to false and rely on the next
	// step to make sure that the sucessor meets the second condition.
    for(unsigned int i = 0; i < phi_bb_terminating_branch->getNumSuccessors(); i++) 
    {
        BasicBlock* successor = phi_bb_terminating_branch->getSuccessor(i);
        if(std::find(associated_loop->block_begin(), associated_loop->block_end(), successor) == associated_loop->block_end()) 
        {
            is_do_loop = false;
            break;
        }
    }

	// XXX! This doesn't make sense to me (-sat).
    for(unsigned int i = 0; i < phi_bb_terminating_branch->getNumSuccessors(); i++) 
    {
        BasicBlock* successor = phi_bb_terminating_branch->getSuccessor(i);
        if(&phi_bb == successor)
		{
            is_do_loop = true;
            break;
        }
    }

    args.push_back(ConstantInt::get(types.i32(), timestampPlacer.getId(phi), false)); // id for destination
    args.push_back(ConstantInt::get(types.i32(), timestampPlacer.getId(controlling_cond), false)); // id for controlling cond
#if 0
    push_int(timestampPlacer.getId(phi)); // id for destination
    push_int(timestampPlacer.getId(controlling_cond)); // id for controlling cond
#endif


	ArrayRef<Value*> *aref = NULL;
    // do..while loops need the condition appended after the loop concludes
    if(is_do_loop)
	{
    	IsPredecessor phi_bb_is_predecessor(&phi_bb);
        for(unsigned int i = 0; i < phi_bb_terminating_branch->getNumSuccessors(); i++) 
        {
            BasicBlock* successor = phi_bb_terminating_branch->getSuccessor(i);
            if(!phi_bb_is_predecessor(successor) && &phi_bb != successor) 
            {
				// TODO: FIXME this looks wrong... phi_bb should be replaced
				// by successor????
				aref = new ArrayRef<Value*>(args);
    			CallInst& ci = *CallInst::Create(addCondFunc, *aref, "");
				delete aref;
				aref = NULL;
                timestampPlacer.constrainInstPlacement(ci, *phi_bb.getTerminator());
    			timestampPlacer.requireValTimestampBeforeUser(controlling_cond, ci);
            }
        }
	}
    // while loops need the condition appended as soon as the header executes
    else
    { 
		aref = new ArrayRef<Value*>(args);
    	CallInst& ci = *CallInst::Create(addCondFunc, *aref, "");
		delete aref;
		aref = NULL;
        timestampPlacer.constrainInstPlacement(ci, *phi_bb.getTerminator());
    	timestampPlacer.requireValTimestampBeforeUser(controlling_cond, ci);
    }
}

/**
 * Handles reduction variables.
 *
 * @param phi The reduction variable.
 */
void PhiHandler::handleReductionVariable(llvm::PHINode& phi)
{
    // TODO: put reduction var.
}

/**
 * Hanldes phi instructions.
 *
 * @param inst The phi instruction.
 */
void PhiHandler::handle(llvm::Instruction& inst)
{
    PHINode& phi = *cast<PHINode>(&inst);

    if(inductionVars.isInductionVariable(phi))
    {
        handleIndVar(phi);
        return;
    }

    if(reductionVars.isReductionVar(&phi))
    {
        handleReductionVariable(phi);
        return;
    }

    std::vector<Value*> log_func_args;
    LLVMTypes types(phi.getContext());
#if 0
    function<void(unsigned int)> push_int = bind(&vector<Value*>::push_back, 
        ref(log_func_args), bind<Constant*>(&ConstantInt::get, types.i32(), _1, false));
#endif

    LOG_DEBUG() << "processing phi node: " << PRINT_VALUE(inst) << "\n";

    // Destination ID
    log_func_args.push_back(ConstantInt::get(types.i32(), timestampPlacer.getId(phi), false));
#if 0
    push_int(timestampPlacer.getId(phi));
#endif

    // Incoming Value ID.
    PHINode& incoming_val_id = identifyIncomingValueId(phi);
    timestampPlacer.constrainInstPlacement(incoming_val_id, phi);
    log_func_args.push_back(&incoming_val_id);

	// Create call to KPhi based on the number of control deps associated with
	// the phi. For speed, we have several specialized versions with common
	// numbers of control deps to avoid overhead of var arg usage.
    std::vector<PHINode*> ctrl_deps;
    getConditions(phi, ctrl_deps);

    Function* phi_logging_func = this->phiLoggingFunc;

    // If we have specialized version, use that func.
    unsigned int num_ctrl_deps = ctrl_deps.size();
    SpecializedLogFuncs::iterator it = specializedPhiLoggingFuncs.find(num_ctrl_deps);
    if(it != specializedPhiLoggingFuncs.end())
        phi_logging_func = it->second;

    // Otherwise, use var arg and set the num of args.
    else {
    	log_func_args.push_back(ConstantInt::get(types.i32(), num_ctrl_deps, false));
#if 0
        push_int(num_ctrl_deps);
#endif
    }

    // Push on all the ctrl deps.
    foreach(PHINode* ctrl_dep, ctrl_deps)
    {
        log_func_args.push_back(ctrl_dep);       // Add the condition id to the _KPhi
        timestampPlacer.constrainInstPlacement(*ctrl_dep, phi);  // Force phi before call to KPhi 
    }

    // Make and add the call.
	ArrayRef<Value*> *aref = new ArrayRef<Value*>(log_func_args);
    CallInst& ci = *CallInst::Create(phi_logging_func, *aref, "");
	delete aref;
    timestampPlacer.constrainInstPlacement(ci, *phi.getParent()->getFirstNonPHI());

    // TODO: Causes problem in this case
    //
    // %0 = phi ...
    // _KPhi(%reg0, ...)
    // %1 = icmp %0, ...
    // _KPhiAddCond(%reg0, %timestamp_of_icmp)
    // br ...
    //
    // This gets the timestamp of the phi as %reg0. Then we perform a compare
    // that depends on the value of the phi. Consequently, the timestamp of
    // the compare is one more than the phi. However, the icmp can be 
    handleLoops(phi);
}

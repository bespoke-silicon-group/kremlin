#include <boost/assign/std/vector.hpp>
#include <boost/assign/std/set.hpp>
#include <boost/lexical_cast.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include "PhiHandler.h"
#include "LLVMTypes.h"

#define MAX_SPECIALIZED 5

using namespace llvm;
using namespace boost;
using namespace boost::assign;
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

        return results;
    }

};

/**
 * Constructs a new phi instruction handler.
 *
 * @param ts_placer The timestamp placer this handler is associated with.
 */
PhiHandler::PhiHandler(TimestampPlacer& ts_placer) :
    cd(ts_placer.getAnalyses().cd),
    dt(ts_placer.getAnalyses().dt),
    ind_vars(ts_placer.getAnalyses().li),
    li(ts_placer.getAnalyses().li),
    log(PassLog::get()),
    reduction_vars(ts_placer.getAnalyses().rv),
    ts_placer(ts_placer)
{
    // Set up the opcodes
    opcodes += Instruction::PHI;

    // Setup the log_func
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> args;
    args += types.i32(), types.i32(), types.i32();
    FunctionType* func_type = FunctionType::get(types.voidTy(), args, true);
    log_func = cast<Function>(m.getOrInsertFunction("_KPhi", func_type));

    // Setup specialized funcs
    args.clear();
    args += types.i32(), types.i32();
    for(size_t i = 1; i < MAX_SPECIALIZED; i++)
    {
        args += types.i32();
        FunctionType* type = FunctionType::get(types.voidTy(), args, false);
        Function* func = cast<Function>(m.getOrInsertFunction(
                "_KPhi" + lexical_cast<string>(i) + "To1", type));
        log_funcs.insert(make_pair(i, func));
    }

    // add_cond_func
    args.clear();
    args += types.i32(), types.i32();
    FunctionType* add_cond_type = FunctionType::get(types.voidTy(), args, false);
    add_cond_func = cast<Function>(m.getOrInsertFunction(
            "_KPhiAddCond", add_cond_type));

    // induc_var_func
    args.clear();
    args += types.i32();
    FunctionType* induc_var_type = FunctionType::get(types.voidTy(), args, false);
    induc_var_func = cast<Function>(m.getOrInsertFunction(
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
    LLVMTypes types(phi.getContext());

    std::vector<Value*> args;

    // If this a PHI that is a loop induction var, we only logAssignment when
    // we first enter the loop so that we don't get caught in a dependency
    // cycle. For example, the induction var inc would get the time of t_init
    // for the first iter, which would then force the next iter's t_init to be
    // one higher and so on and so forth...
    LOG_DEBUG() << "processing canonical induction var: " << phi << "\n";

    // Loop through all incoming vals of PHI and find the one that is constant (i.e. initializes the loop to 0).
    // We then insert a call to logAssignmentConst() to permanently set the time of this to whatever t_init happens
    // to be in the BB where that value comes from.
    int const_idx = -1;

    for(unsigned int n = 0; n < phi.getNumIncomingValues(); ++n) {
        if(isa<ConstantInt>(phi.getIncomingValue(n)) && const_idx == -1) {
            const_idx = n;
        }
        else if(isa<ConstantInt>(phi.getIncomingValue(n))) {
            LOG_ERROR() << "found multiple incoming constants to induction var phi node\n";
            log.close();
            assert(0);
        }
    }

    if(const_idx == -1) {
        LOG_ERROR() << "could not find constant incoming value to induction variable phi node\n";
        log.close();
        assert(0);
    }

    BasicBlock& in_blk = *phi.getIncomingBlock(const_idx);

    // finally, we will insert the call to logAssignmentConst
    args.push_back(ConstantInt::get(types.i32(), ts_placer.getId(phi), false));
    CallInst& ci = *CallInst::Create(induc_var_func, args.begin(), args.end(), "");
    ts_placer.add(ci, *in_blk.getTerminator());
}

/**
 * @return A phi node containing the virtual register table index of the
 * incoming value's timestamp.
 */
PHINode& PhiHandler::identifyIncomingValueId(PHINode& phi)
{
    LLVMTypes types(phi.getContext());
    PHINode& incoming_val_id = *PHINode::Create(types.i32(), "phi-incoming-val-id");

    for(unsigned int i = 0; i < phi.getNumIncomingValues(); i++) 
    {

        BasicBlock& incoming_block = *phi.getIncomingBlock(i);
        Value& incoming_val = *phi.getIncomingValue(i);

        unsigned int incoming_id = 0; // default ID to 0 (i.e. a constant)

        // if incoming val isn't a constant, we grab the it's ID
        if(!isa<Constant>(&incoming_val))
        {
            incoming_id = ts_placer.getId(incoming_val);
            ts_placer.requestTimestamp(incoming_val, *incoming_block.getTerminator());
        }

        incoming_val_id.addIncoming(ConstantInt::get(types.i32(),incoming_id), &incoming_block);
    }

    return incoming_val_id;
}

/**
 * @param bb The block to find the control timestamp index.
 * @return the control condition's virtual register table timestamp index. 
 * This will be the condition in bb.
 */
unsigned int PhiHandler::getConditionIdOfBlock(llvm::BasicBlock& bb)
{
    Value& controlling_cond = *cd.getControllingCondition(&bb);
    ts_placer.requestTimestamp(controlling_cond, *bb.getTerminator());
    return ts_placer.getId(controlling_cond);
}

/**
 * Returns all of the condtions the phi node depends on.
 *
 * @return A vector of phi nodes. Each of the phi nodes contains the virtual
 * register table index of the timestamp of the condition or 0 if the
 * condition is not dominated for the given branch.
 */
vector<PHINode*>& PhiHandler::getConditions(PHINode& phi, vector<PHINode*>& control_deps)
{
    LLVMTypes types(phi.getContext());
    BasicBlock& bb = *phi.getParent();

    // Get all the controllers of every block from the one containing the phi
    // to the block that dominates the phi.
    std::set<BasicBlock*> controllers;
    cd.getPhiControllingBlocks(phi, controllers);

    foreach(BasicBlock* controller, controllers)
    {
        LOG_DEBUG() << "controller to " << bb.getName() << ": " << controller->getName() << "\n";
        
        PHINode* incoming_condition_addr = PHINode::Create(types.i32(),"phi-incoming-condition");

        std::map<BasicBlock*, Value*> incoming_value_addrs;

        bool at_least_one_controller = false;

        // check all preds of current basic block
        for(unsigned int i = 0; i < phi.getNumIncomingValues(); i++) 
        {
            BasicBlock* incoming_block = phi.getIncomingBlock(i);
            unsigned int incoming_id = 0;

            // If this controller dominates incoming block, get the id of condition at end of pred
            if(dt.dominates(controller, incoming_block)) 
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
    function<void(unsigned int)> push_int = bind(&vector<Value*>::push_back, 
        ref(args), bind<Constant*>(&ConstantInt::get, types.i32(), _1, false));

    BasicBlock& bb = *phi.getParent();
    IsPredecessor is_predecessor(&bb);

    // Only handle loops.
    if(!li.isLoopHeader(&bb))
        return;

    Loop* loop = li.getLoopFor(&bb);

    // Only try if this branches loop header branches to multiple targets.
    TerminatorInst* terminator = bb.getTerminator();
    BranchInst* branch_inst;
    if(!(branch_inst = dyn_cast<BranchInst>(terminator)) || !branch_inst->isConditional())
        return;

    // No need to add conditions if it happens to be a constant. 
    // (e.g. while(true))
    Value& controlling_cond = *cd.getControllingCondition(&bb);
    if(isa<Constant>(&controlling_cond))
        return;

    // Only looks like do loops if there are successors outside the
    // loop and the successor is not the same block.
    bool is_do_loop = true;
    for(unsigned int i = 0; i < branch_inst->getNumSuccessors(); i++) 
    {
        BasicBlock* successor = branch_inst->getSuccessor(i);
        if(std::find(loop->block_begin(), loop->block_end(), successor) == loop->block_end()) 
        {
            is_do_loop = false;
            break;
        }
    }
    for(unsigned int i = 0; i < branch_inst->getNumSuccessors(); i++) 
    {
        BasicBlock* successor = branch_inst->getSuccessor(i);
        if(&bb == successor) {
            is_do_loop = true;
            break;
        }
    }

    // Destination and control ID
    push_int(ts_placer.getId(phi));
    push_int(ts_placer.getId(controlling_cond));

    // do..while loops need the condition appended after the loop concludes
    if(is_do_loop)
        for(unsigned int i = 0; i < branch_inst->getNumSuccessors(); i++) 
        {
            BasicBlock* successor = branch_inst->getSuccessor(i);
            if(!is_predecessor(successor) && &bb != successor) 
            {
                CallInst& ci = *CallInst::Create(add_cond_func, args.begin(), args.end(), "");
                ts_placer.add(ci, *bb.getTerminator());
                ts_placer.requestTimestamp(controlling_cond, ci);
            }
        }

    // while loops need the condition appended as soon as the header executes
    else
    { 
        CallInst& ci = *CallInst::Create(add_cond_func, args.begin(), args.end(), "");
        ts_placer.add(ci, *bb.getTerminator());
        ts_placer.requestTimestamp(controlling_cond, ci);
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

    if(ind_vars.isInductionVariable(phi))
    {
        handleIndVar(phi);
        return;
    }

    if(reduction_vars.isReductionVar(&phi))
    {
        handleReductionVariable(phi);
        return;
    }

    std::vector<Value*> args;
    LLVMTypes types(phi.getContext());
    function<void(unsigned int)> push_int = bind(&vector<Value*>::push_back, 
        ref(args), bind<Constant*>(&ConstantInt::get, types.i32(), _1, false));

    LOG_DEBUG() << "processing phi node: " << PRINT_VALUE(*i) << "\n";

    // Destination ID
    push_int(ts_placer.getId(phi));

    // Incoming Value ID.
    PHINode& incoming_val_id = identifyIncomingValueId(phi);
    ts_placer.add(incoming_val_id, phi);
    args.push_back(&incoming_val_id);

    std::vector<PHINode*> ctrl_deps;
    getConditions(phi, ctrl_deps);

    IsPredecessor is_predecessor(phi.getParent());
    unsigned int num_ctrl_deps = ctrl_deps.size();

    SpecializedLogFuncs::iterator it = log_funcs.find(num_ctrl_deps);
    Function* log_func = this->log_func;

    // If specialized, use that func.
    if(it != log_funcs.end())
        log_func = it->second;

    // Otherwise, use var arg and set the num of args.
    else
        push_int(num_ctrl_deps);

    // Push on all the ctrl deps.
    foreach(PHINode* ctrl_dep, ctrl_deps)
    {
        args.push_back(ctrl_dep);       // Add the condition id to the logPhi
        ts_placer.add(*ctrl_dep, phi);  // Make sure the phi instruction comes before the logPhi. 
    }

    // Make and add the call.
    CallInst& ci = *CallInst::Create(log_func, args.begin(), args.end(), "");
    ts_placer.add(ci, *phi.getParent()->getFirstNonPHI());

    args.clear();

    // TODO: Causes problem in this case
    //
    // %0 = phi ...
    // logPhi(%reg0, ...)
    // %1 = icmp %0, ...
    // logPhiAddCondition(%reg0, %timestamp_of_icmp)
    // br ...
    //
    // This gets the timestamp of the phi as %reg0. Then we perform a compare
    // that depends on the value of the phi. Consequently, the timestamp of
    // the compare is one more than the phi. However, the icmp can be 
    handleLoops(phi);
}

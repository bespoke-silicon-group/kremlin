#include "analysis/ReductionVars.h"

#include <llvm/IR/Module.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>
#include "foreach.h"

using namespace llvm;

/// ID for opt.
char ReductionVars::ID = 0xdeadbeef;

/**
 * Constructs a new analysis pass. Should not be called except by opt.
 */
ReductionVars::ReductionVars() :
    FunctionPass(ID),
    log(PassLog::get())
{
    LOG_DEBUG() << "Constructing ReductionVars with id: " << (int)ID << "\n";
}

ReductionVars::~ReductionVars()
{
    LOG_DEBUG() << "Destructing ReductionVars with id: " << (int)ID << "\n";
}

/**
 * Analyzes a function.
 *
 * @param f The function to analyze.
 */
bool ReductionVars::runOnFunction(llvm::Function &f)
{
    if(f.isDeclaration())
        return false;

    LoopInfo& loop_info = getAnalysis<LoopInfo>();
    for(LoopInfo::iterator loop = loop_info.begin(), loop_end = loop_info.end(); loop != loop_end; ++loop)
        getReductionVars(loop_info, *loop);

    return false; // We did not modify anything.
}

/**
 * @return true of the instruction is a reduction variable.
 */
bool ReductionVars::isReductionVar(llvm::Instruction* inst) const
{
    return red_var_ops.find(inst) != red_var_ops.end();
}

/**
 * @return true If the instruction has the type of a reduction var.
 */
bool ReductionVars::isReductionOpType(Instruction* inst)
{
    if( inst->getOpcode() == Instruction::Add
      || inst->getOpcode() == Instruction::FAdd
      || inst->getOpcode() == Instruction::Mul
      || inst->getOpcode() == Instruction::FMul
      ) { return true; }
    else { return false; }
}

/**
 * @param li The loop info.
 * @param loop The loop associated with li.
 * @return A vector of non-phi instructions that are in loop (but not a subloop of loop) and use the Value val.
 */
std::vector<Instruction*> ReductionVars::getNonPhiUsesInLoop(LoopInfo& li, Loop* loop, Value* val)
{
    std::vector<Instruction*> uses;

    for(Value::use_iterator ui = val->use_begin(), ue = val->use_end(); ui != ue; ++ui) {
        if(isa<Instruction>(*ui)) {
            Instruction* inst = cast<Instruction>(*ui);

            if(!isa<PHINode>(inst) && loop == li.getLoopFor(inst->getParent())) {
                uses.push_back(inst);
            }
        }
    }
    return uses;
}

/**
 * @param li The loop info.
 * @param loop The loop associated with li.
 * @param val The value to check for reduction.
 * @return The instruction that does the reduction operation or NULL if val
 * is not a reduction variable.
 */
Instruction* ReductionVars::getReductionVarOp(LoopInfo& li, Loop* loop, Value *val)
{
    std::vector<Instruction*> uses_in_loop = getNonPhiUsesInLoop(li,loop,val);

    bool has_reduction_var_use_pattern =  uses_in_loop.size() == 2
     && isa<LoadInst>(uses_in_loop[1])
     && isa<StoreInst>(uses_in_loop[0])
     && uses_in_loop[1]->hasOneUse();

	if (has_reduction_var_use_pattern) {
        Instruction* load_user = cast<Instruction>(*uses_in_loop[1]->use_begin());
        //LOG_DEBUG() << "\tuser of load: " << PRINT_VALUE(*load_user);

        if( load_user->hasOneUse()
          && uses_in_loop[0] == *load_user->use_begin()
          && isReductionOpType(load_user)
          ) {
            //LOG_DEBUG() << "\t\thot diggity dawg, that is it!\n";
            return load_user;
        }
    }


    return NULL;
}

/**
 * @param li The loop info.
 * @param loop The loop associated with li.
 * @param val The value to check for reduction.
 * @return The instruction that does the reduction operation or NULL if val
 * is not a reduction variable.
 */
Instruction* ReductionVars::getArrayReductionVarOp(LoopInfo& li, Loop* loop, Value *val)
{
    std::vector<Instruction*> uses_in_loop = getNonPhiUsesInLoop(li,loop,val);

    // should have two uses: load then store
    if(uses_in_loop.size() != 2) return NULL;

	// By construction, we know that the two users are GEP insts Note: order
	// is reversed when doing getNonPhiUsesInLoop, so user0 is actually entry
	// 1 and vice versa
    GetElementPtrInst* user0_gep = dyn_cast<GetElementPtrInst>(uses_in_loop[1]);
    GetElementPtrInst* user1_gep = dyn_cast<GetElementPtrInst>(uses_in_loop[0]);

    if(!user0_gep->hasOneUse() || !user1_gep->hasOneUse()) return NULL;

    Instruction* should_be_load = cast<Instruction>(*user0_gep->use_begin());

    if(!isa<LoadInst>(should_be_load)) { return NULL; }
    else if(!isa<StoreInst>(*user1_gep->use_begin())) { return NULL; }

    Instruction* load_user = cast<Instruction>(*should_be_load->use_begin());

    if( load_user->hasOneUse()
      // TODO: make sure load_user is stored?
      && isReductionOpType(load_user)
      ) {
        return load_user;
    }

    return NULL;
}

/**
 * @param li The loop info.
 * @param loop The loop associated with li.
 * @param[in/out] red_var_ops Set of instructions that are reduction variable operators.
 */
void ReductionVars::getArrayReductionVars(LoopInfo& li, Loop* loop, std::set<Instruction*>& red_var_ops)
{
	// The pattern we'll look for is a pointer used in 2 getelementptr insts
	// that are in the same block, with the first being used by a load, the
	// second by a store, and the load being used by a "reduction-friendly"
	// op.

    std::vector<BasicBlock*> loop_blocks = loop->getBlocks();
    for(unsigned j = 0; j < loop_blocks.size(); ++j) {
        BasicBlock* bb = loop_blocks[j];

        std::map<Value*,std::vector<GetElementPtrInst*> > ptr_val_to_geps;

        // create map from pointer values to GEPs that use them
        for(BasicBlock::iterator inst = bb->begin(), inst_end = bb->end(); inst != inst_end; ++inst) {
            if(GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(inst)) {
                ptr_val_to_geps[gep->getPointerOperand()].push_back(gep);
            }
        }

        for(std::map<Value*,std::vector<GetElementPtrInst*> >::iterator gp_it = ptr_val_to_geps.begin(), gp_end = ptr_val_to_geps.end(); gp_it != gp_end; ++gp_it) {
            Instruction* red_var_op = NULL;

			std::vector<GetElementPtrInst*> gep_vector = (*gp_it).second;

			// If this mapped to only one GEP, we'll see if that GEP is used
			// by load and store as part of reduction op sequence.
            if(gep_vector.size() == 1) {
                red_var_op = getReductionVarOp(li,loop,(*gp_it).second.front());
            }
			// If this mapped to two GEPs, we'll use the "Array" formulation
			// of reduction variables to see if this value's GEPS are used by
			// a load and store (respectively) that are part of a reduction op
			// sequence.
#if 0
            else if(gep_vector.size() == 2) {
				Value* ptr_val = (*gp_it).first;
                red_var_op = getArrayReductionVarOp(li,loop,ptr_val);
            }
#endif

            if(red_var_op) {
                LOG_INFO() << "identified reduction variable operator (array, used in function: "
					<< red_var_op->getParent()->getParent()->getName()
					<< "): " << *red_var_op << "\n"
                	<< "\treduction var: " << *(*gp_it).first << "\n";
                red_var_ops.insert(red_var_op);
            }
        }
    }
}

/**
 * @param li The loop info.
 * @param loop The loop associated with li.
 */
void ReductionVars::getReductionVars(LoopInfo& li, Loop* loop)
{
    BasicBlock* header = loop->getHeader();

    // Check all subloops for reduction vars
    std::vector<Loop*> sub_loops = loop->getSubLoops();
	for(unsigned i = 0; i < sub_loops.size(); ++i) {
        getReductionVars(li,sub_loops[i]);
	}
	/*
    if(sub_loops.size() > 0) {
        for(std::vector<Loop*>::iterator sl_it = sub_loops.begin(), sl_end = sub_loops.end(); sl_it != sl_end; ++sl_it) 
            getReductionVars(li,*sl_it);
    }
	*/

	// If there weren't any subloops, we'll examine all the global variables
	// to see if they are reduction variables.
	// We also need to check for a special type of "array" reduction that
	// requires a different type of identification.
    if(sub_loops.empty()) {
        Module* mod = header->getParent()->getParent();

        // Examine each global variable to see if it's a reduction var.
        for(Module::global_iterator gi = mod->global_begin(), ge = mod->global_end(); gi != ge; ++gi) {
            if(Instruction* red_var_op = getReductionVarOp(li,loop,gi)) {
                LOG_INFO() << "identified reduction variable operator (global, used in function: "
				 << red_var_op->getParent()->getParent()->getName() 
				 << "): " << *red_var_op << "\n"
                 << "\treduction var: " << *gi << "\n";
                red_var_ops.insert(red_var_op);
            }
        }

        // Check for "array" reduction.
        getArrayReductionVars(li,loop,red_var_ops);
    }

    PHINode* civ = loop->getCanonicalInductionVariable();

    // Examine all phi nodes in the header (excluding those associated with
	// induction variables) to see if they are PHIs for reduction vars.
	// We consider it to be a reduction var if it has a single use inside this
	// loop and that use is a op that is commutative
    for(BasicBlock::iterator phi_it = header->begin(), phi_end = header->getFirstNonPHI(); phi_it != phi_end; ++phi_it) {
        // ignore induction variables
        if(cast<Instruction>(phi_it) == civ) { continue; }

        // get uses that are in the loop and make sure there is only 1
        std::vector<Instruction*> uses_in_loop = getNonPhiUsesInLoop(li,loop,phi_it);

		// check for the correct "reduction var signature"
        if(uses_in_loop.size() == 1) {
            Instruction* user = uses_in_loop[0];

			if(isReductionOpType(user)) {
                LOG_INFO() << "identified reduction variable operator (phi, function: "
				 << user->getParent()->getParent()->getName()
				 << "): " << *user << "\n"
                 << "\treduction var: " << *phi_it << "\n";
                red_var_ops.insert(user);
            }
        }
    }
}

/**
 * Gets the analysis usage as needed by opt.
 */
void ReductionVars::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.setPreservesAll();
    AU.addRequired<LoopInfo>();
}

static RegisterPass<ReductionVars> X("reduction-vars", "Reduction Variable Analysis", false, true);

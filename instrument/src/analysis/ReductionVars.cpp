#include "analysis/ReductionVars.h"

#include <llvm/Module.h>
#include <llvm/Analysis/LoopInfo.h>
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
bool ReductionVars::isReductionVar(llvm::Instruction* inst) const {
    return red_var_ops.find(inst) != red_var_ops.end();
}

/**
 * @return true If the instruction has the type of a reduction var.
 */
bool ReductionVars::isReductionOpType(Instruction* inst) {
    if( inst->getOpcode() == Instruction::Add
      || inst->getOpcode() == Instruction::FAdd
      || inst->getOpcode() == Instruction::Sub
      || inst->getOpcode() == Instruction::FSub
      || inst->getOpcode() == Instruction::Mul
      || inst->getOpcode() == Instruction::FMul
      ) { return true; }
    else { return false; }
}

/**
 * @param li The loop info.
 * @param loop The loop associated with the loop info.
 * @return A vector of instructions that are in loop (but not a subloop of loop) and use the Value val.
 */
std::vector<Instruction*> ReductionVars::getUsesInLoop(LoopInfo& li, Loop* loop, Value* val) {
    std::vector<Instruction*> uses;

    for(Value::use_iterator ui = val->use_begin(), ue = val->use_end(); ui != ue; ++ui) {
        //LOG_DEBUG() << "\tchecking use: " << **ui;

        if(isa<Instruction>(*ui)) {
            Instruction* i = cast<Instruction>(*ui);
            //Loop* user_loop = li.getLoopFor(i->getParent());

            //LOG_DEBUG() << "\t\tparent loop for use has header " << user_loop->getHeader()->getName() << "\n";

            if(!isa<PHINode>(i) && loop == li.getLoopFor(i->getParent())) {
                uses.push_back(i);
            }
        }
    }
    
    return uses;
}

llvm::Instruction* ReductionVars::getReductionVarOp(llvm::LoopInfo& li, llvm::Loop* loop, llvm::Value *val) {
    std::vector<Instruction*> uses_in_loop = getUsesInLoop(li,loop,val);

    //LOG_DEBUG() << "\tnum uses in loop: " << uses_in_loop.size() << "\n";


    if(uses_in_loop.size() == 2
     && isa<LoadInst>(uses_in_loop[1])
     && isa<StoreInst>(uses_in_loop[0])
     && uses_in_loop[1]->hasOneUse()
     ) {
        //LOG_DEBUG() << "\tlooking good so far for: " << PRINT_VALUE(*val);
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

Instruction* ReductionVars::getArrayReductionVarOp(LoopInfo& li, Loop* loop, Value *val) {
    std::vector<Instruction*> uses_in_loop = getUsesInLoop(li,loop,val);

    // should have two uses: load then store
    if(uses_in_loop.size() != 2) {
        //LOG_DEBUG() << "\tdidn't have 2 uses\n";
        return NULL;
    }

    // By construction, we know that the two users are GEP insts
    // Node: order is reversed when doing getUsesInLoop, so user0 is actually entry 1
    // and vice versa
    GetElementPtrInst* gep_user0 = dyn_cast<GetElementPtrInst>(uses_in_loop[1]);
    GetElementPtrInst* gep_user1 = dyn_cast<GetElementPtrInst>(uses_in_loop[0]);

    if(gep_user0->getNumUses() != 1) {
        //LOG_DEBUG() << "not exactly 1 user of get_user0: " << *gep_user0 << "\n";
        return NULL;
    }
    else if(gep_user1->getNumUses() != 1) {
        //LOG_DEBUG() << "not exactly 1 user of get_user1: " << *gep_user1 << "\n";
        return NULL;
    }

    Instruction* should_be_load = cast<Instruction>(*gep_user0->use_begin());

    if(!isa<LoadInst>(should_be_load)) {
        //LOG_DEBUG() << "\t not a load user: " << *should_be_load << "\n";
        //LOG_DEBUG() << "\t\t get_user1's first user: " << **gep_user1->use_begin() << "\n";
        return NULL;
    }
    else if(!isa<StoreInst>(*gep_user1->use_begin())) {
        //LOG_DEBUG() << "\t not a store user: " << **gep_user1->use_begin() << "\n";
        //LOG_DEBUG() << "\t\t get_user0's first user: " << *should_be_load << "\n";
        return NULL;
    }

    //LOG_DEBUG() << "\tlooking good so far for: " << PRINT_VALUE(*val);
    Instruction* load_user = cast<Instruction>(*should_be_load->use_begin());
    //LOG_DEBUG() << "\tuser of load: " << PRINT_VALUE(*load_user);

    if( load_user->hasOneUse()
      // TODO: make sure load_user is stored?
      && isReductionOpType(load_user)
      ) {
        return load_user;
    }


    return NULL;
}

void ReductionVars::getArrayReductionVars(LoopInfo& li, Loop* loop, std::set<Instruction*>& red_var_ops) {
    // The pattern we'll look for is a pointer used in 2 getelementptr insts that are in the same block,
    // with the first being used by a load, the second by a store, and the load being used by an associative
    // op

    // loop through basic blocks in loop
    std::vector<BasicBlock*> loop_blocks = loop->getBlocks();

    //for(Loop::block_iterator* bb = loop->block_begin(), bb_end = loop->block_end(); bb != bb_end; ++bb) {
    for(unsigned j = 0; j < loop_blocks.size(); ++j) {
        BasicBlock* bb = loop_blocks[j];
        std::vector<GetElementPtrInst*> geps;

        std::map<Value*,std::vector<GetElementPtrInst*> > ptr_val_to_geps;

        // find all the GEP instructions
        for(BasicBlock::iterator inst = bb->begin(), inst_end = bb->end(); inst != inst_end; ++inst) {
            if(GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(inst)) {
                ptr_val_to_geps[gep->getPointerOperand()].push_back(gep);
                //geps.push_back(cast<GetElementPtrInst>(inst));
            }
        }

        //for(std::map<Value*,unsigned>::iterator gp_it = gep_ptr_uses_in_bb.begin(), gp_end = gep_ptr_uses_in_bb.end(); gp_it != gp_end; ++gp_it) {
        for(std::map<Value*,std::vector<GetElementPtrInst*> >::iterator gp_it = ptr_val_to_geps.begin(), gp_end = ptr_val_to_geps.end(); gp_it != gp_end; ++gp_it) {
            Instruction* red_var_op = NULL;

            if((*gp_it).second.size() == 1) {
                red_var_op = getReductionVarOp(li,loop,(*gp_it).second.front());
            }
            else if((*gp_it).second.size() == 2) {
                red_var_op = getArrayReductionVarOp(li,loop,(*gp_it).first);
            }

            if(red_var_op) {
                LOG_WARN() << "identified reduction variable increment (array, used in function: " << red_var_op->getParent()->getParent()->getName() << "): " << *red_var_op << "\n";
                LOG_WARN() << "\treduction var: " << *(*gp_it).first << "\n";
                red_var_ops.insert(red_var_op);
            }
        }
    }
}

void ReductionVars::getReductionVars(LoopInfo& li, Loop* loop) {
    BasicBlock* header = loop->getHeader();
    LOG_DEBUG() << "checking for red vars in loop headed by " << header->getName() << " (func = " << header->getParent()->getName() << ")\n";

    std::vector<Loop*> sub_loops = loop->getSubLoops();

    // check all subloops for reduction vars
    if(sub_loops.size() > 0) {
        for(std::vector<Loop*>::iterator sl_it = sub_loops.begin(), sl_end = sub_loops.end(); sl_it != sl_end; ++sl_it) 
            getReductionVars(li,*sl_it);
    }

    // If no subloops we can look for global vars with "load; comm op; store" pattern.
    // We also check for array reductions here (which are a bit tricky).
    else {
        // check for global var reduction
        Module* mod = header->getParent()->getParent();

        // loop through all global vars
        for(Module::global_iterator gi = mod->global_begin(), ge = mod->global_end(); gi != ge; ++gi) {
            // XXX hack to only look at rho variable now (confirmed as reduction)
            //if(gi->getName().find("rho") == std::string::npos) continue;

            //LOG_DEBUG() << "checking for red var: " << *gi;

            if(Instruction* red_var_op = getReductionVarOp(li,loop,gi)) {
                LOG_DEBUG() << "identified reduction variable increment (global, used in function: " << red_var_op->getParent()->getParent()->getName() << "): " << *red_var_op << "\n";
                LOG_DEBUG() << "\treduction var: " << *gi << "\n";
                red_var_ops.insert(red_var_op);
            }
        }

        // check for array reduction.
        getArrayReductionVars(li,loop,red_var_ops);
    }

    //LOG_DEBUG() << "checking for red vars in loop with header " << header->getName() << "\n";

    PHINode* civ = loop->getCanonicalInductionVariable();

    // examine all phi nodes in the header to see if they are phis for reduction vars
    // We consider it to be a reduction var if it has a single use inside this loop and that
    // use is a op that is commutative (e.g. add, sub, or mul)
    for(BasicBlock::iterator phi_it = header->begin(), phi_end = header->getFirstNonPHI(); phi_it != phi_end; ++phi_it) {
        // we don't want induction vars to be considered
        if(cast<Instruction>(phi_it) == civ) { continue; }

        //LOG_DEBUG() << "checking for red var: " << *phi_it;

        // get uses that are in the loop and make sure there is only 1
        std::vector<Instruction*> uses_in_loop = getUsesInLoop(li,loop,phi_it);

        //LOG_DEBUG() << "\t... has " << uses_in_loop.size() << " uses\n";

        if(uses_in_loop.size() == 1) {
            Instruction* user = uses_in_loop[0];

            // TODO: are there other commutative ops we are missing?
            if(user->getOpcode() == Instruction::Add
              || user->getOpcode() == Instruction::FAdd
              || user->getOpcode() == Instruction::Sub
              || user->getOpcode() == Instruction::FSub
              || user->getOpcode() == Instruction::Mul
              || user->getOpcode() == Instruction::FMul
              ) {
                LOG_DEBUG() << "identified reduction variable increment (phi, function: " << user->getParent()->getParent()->getName() << "): " << *user << "\n";
                LOG_DEBUG() << "\treduction var: " << *phi_it << "\n";
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
    AU.addRequired<LoopInfo>();
    AU.setPreservesAll();
}

INITIALIZE_PASS(ReductionVars, "reduction-vars", "Reduction Variable Analysis", false, true);

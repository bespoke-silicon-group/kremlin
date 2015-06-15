#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstrTypes.h"

#include <map>
#include <set>

#include "PassLog.h"


using namespace llvm;

namespace {
	static cl::opt<unsigned int> minChainLength("min-chain-length",cl::desc("Sets the minimum length for a dependency chain to be considered before breaking."),cl::Hidden,cl::init(3));

	struct AssociativeDependenceBreak : public BasicBlockPass {
		static char ID;
		PassLog& log;

		AssociativeDependenceBreak() : BasicBlockPass(ID), log(PassLog::get()) {}

		enum chainStatus { NONE, LINK, END };

		virtual bool runOnBasicBlock(BasicBlock &BB) {
			bool was_changed = false;

			//LOG_DEBUG() << "BB:\n" << PRINT_VALUE(BB) << "\n";

			// Determine which nodes can be links in our chains of dep instructions.
			// We also note which ones can ONLY be the end of a chain.
			std::map<Value*,chainStatus> chain_status; // 0 = not in chain, 1 = chain link (or end?), 2 = chain end only

			std::vector<Instruction*> candidate_insts;
			for(BasicBlock::iterator inst = BB.begin(), inst_end = BB.end(); inst != inst_end; ++inst) {
				unsigned int op_type = inst->getOpcode();
				//LOG_DEBUG() << "checking for basic criteria: " << PRINT_VALUE(*inst) << "\n";

				// Op must be associative (e.g. add or mul)
				// AND not be a pointer type (to avoid dealing with aliasing stuff)
				if((op_type == Instruction::Add
				  || op_type == Instruction::FAdd
				  || op_type == Instruction::Mul
				  || op_type == Instruction::FMul)
				  && !isa<PointerType>(inst->getType())
				  ) {
					// if it only has one use then it's a link
					if(inst->hasOneUse()) {
						//LOG_DEBUG() << "\tpotential chain link\n";
						chain_status[inst] = LINK;
						candidate_insts.push_back(inst);
					}
					// if it has more than 1 use then it can only be end of chain
					else if(inst->getNumUses() > 1) {
						//LOG_DEBUG() << "\tpotential chain end (multi-user)\n";
						chain_status[inst] = END;
						candidate_insts.push_back(inst);
					}
					// no uses???
					else {
						chain_status[inst] = NONE;
					}
				}
				else {
					chain_status[inst] = NONE;
				}
			}

			if(candidate_insts.empty()) { return false; }


			// We now check the chain link candidates to see which ones have users outside the list
			// of candidates or whose users have a different op type. These candidates
			// become chain_end_only candidates.
			for(BasicBlock::iterator inst = BB.begin(), inst_end = BB.end(); inst != inst_end; ++inst) {
				if(chain_status[inst] != LINK) continue;

				Instruction* first_user = dyn_cast<Instruction>(*(inst->use_begin()));

				// If the user isn't in this BB then we know it has to be an end.
				// Likewise, if user isn't in the chain, it can only be an end.
				if(first_user->getParent() != &BB
				    || chain_status[first_user] == NONE
					|| first_user->getOpcode() != inst->getOpcode()
				  ) {
					//LOG_DEBUG() << "changing status to link end: " << PRINT_VALUE(*inst) << "\n";
					chain_status[inst] = END;
				}
			}

			// now we'll try to build the chain of insts
			std::set<std::vector<Instruction*> > set_of_chains;
			std::set<Instruction*> used_insts;

			for(BasicBlock::iterator inst = BB.begin(), inst_end = BB.end(); inst != inst_end; ++inst) {
				if(used_insts.find(inst) != used_insts.end()
					|| chain_status[inst] == NONE
				  ) { continue; }

				//LOG_DEBUG() << "checking for start of chain at: " << PRINT_VALUE(*inst) << "\n";

				std::vector<Instruction*> inst_chain;
				Instruction* curr_inst = inst;

				while(chain_status[curr_inst] == LINK) {
					inst_chain.push_back(curr_inst);

					curr_inst = dyn_cast<Instruction>(*(curr_inst->use_begin()));
					//LOG_DEBUG() << "next link in chain: " << PRINT_VALUE(*curr_inst) << "\n";
				}

				assert(chain_status[curr_inst] == END && "End of chain not marked as end.");

				// don't forget to add the chain end link
				inst_chain.push_back(curr_inst);

				if(inst_chain.size() >= minChainLength) {
					LOG_DEBUG() << "found chain with " << inst_chain.size() << " links:\n";

					set_of_chains.insert(inst_chain);

					for(unsigned i = 0; i < inst_chain.size(); ++i) {
						used_insts.insert(inst_chain[i]);
						LOG_DEBUG() << "\t" << PRINT_VALUE(*inst_chain[i]) << "\n";
					}
				}
			}

			for(std::set<std::vector<Instruction*> >::iterator ci = set_of_chains.begin(), ce = set_of_chains.end(); ci != ce; ++ci) {
				//LOG_DEBUG() << "processing next chain of insts\n";
				was_changed = true;

				std::vector<Instruction*> chain = *ci;

				/*
				for(unsigned i = 0; i < chain.size(); ++i) {
					LOG_DEBUG() << "\t" << PRINT_VALUE(*chain[i]) << "\n";
				}
				*/

				std::vector<Value*> curr_chain;
				curr_chain.assign(chain.begin(),chain.end());

				BinaryOperator* bin_op = dyn_cast<BinaryOperator>(chain.front()); // dyn_cast is safe here because only binops are allowed

				// Populate next_chain for all the operands that are input into the curr_chain
				std::vector<Value*> next_chain;

				// First element will always have both operands in the set (since it starts the chain
				// of deps). We'll go ahead and just put it directly in next_chain.
				next_chain.push_back(curr_chain[0]);

				// exactly one of the two operands will be in the next_chain, the other is
				// just the continuation of the serial chain
				//LOG_DEBUG() << "external inputs to chain:\n";
				for(unsigned i = 1; i < curr_chain.size(); ++i) {
					Instruction* curr_inst = dyn_cast<Instruction>(curr_chain[i]);
					if(curr_inst->getOperand(0) == curr_chain[i-1]) {
						// op 1 is the input
						next_chain.push_back(curr_inst->getOperand(1));
						//LOG_DEBUG() << "\t" << PRINT_VALUE(*curr_inst->getOperand(1)) << "\n";
					}
					else {
						// op 0 is the input
						next_chain.push_back(curr_inst->getOperand(0));
						//LOG_DEBUG() << "\t" << PRINT_VALUE(*curr_inst->getOperand(0)) << "\n";
					}

					// erase curr_inst because it will no longer be used
					//curr_inst->removeFromParent();
				}

				while(next_chain.size() != 1) {
					// copy next_chain over to be the curr_chain
					curr_chain.assign(next_chain.begin(),next_chain.end());

					while(!next_chain.empty()) next_chain.pop_back();

					// We can only handle an even number of insts so we'll push the last inst of
					// curr_chain to the front of next_chain if we have an odd number currently.
					if(curr_chain.size() % 2 == 1) {
						next_chain.push_back(curr_chain.back());
						curr_chain.pop_back();
					}

					// Form new insts by grouping 2 consec elements of curr_chain together
					// These new insts will go onto the next_chain.
					for(unsigned i = 0; i < curr_chain.size(); i += 2) {
						Instruction* new_inst = BinaryOperator::Create(bin_op->getOpcode(),curr_chain[i],curr_chain[i+1],"adb",chain[chain.size()-1]);
						//LOG_DEBUG() << "created new instruction: " << PRINT_VALUE(*new_inst) << "\n";

						next_chain.push_back(new_inst);
					}
				}

				// Replace uses of the last value in chain with sole member of next_chain
				chain.back()->replaceAllUsesWith(next_chain[0]);

				// start from end of chain and erase old insts
				for(unsigned i = chain.size()-1; i > 0; --i) {
					chain[i]->eraseFromParent();
				}
			}

			//log.close();
			//if(was_changed) { LOG_DEBUG() << "modified BB:\n" << PRINT_VALUE(BB) << "\n"; }

			return was_changed;
		}// end runOnBasicBlock(...)

		void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesCFG();
		}
	};  // end of struct AssociativeDependenceBreak

	char AssociativeDependenceBreak::ID = 0;

	RegisterPass<AssociativeDependenceBreak> X("assoc-dep-break", "Identifies serial associative operations that can be parallelized",
	  false /* Only looks at CFG */,
	  false /* Analysis Pass */);
} // end anon namespace

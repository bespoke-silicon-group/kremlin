#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Instructions.h"
#include "llvm/Instruction.h"
#include "llvm/Constants.h"
#include "llvm/GlobalVariable.h"
#include "llvm/User.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Support/CommandLine.h"
#include <map>

#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"

#include "llvm/Support/CFG.h"

#include <limits.h>

#include "PassLog.h"


using namespace llvm;

namespace {

	struct AssociativeDependenceBreak : public BasicBlockPass {
		static char ID;
		PassLog& log;

		AssociativeDependenceBreak() : BasicBlockPass(ID), log(PassLog::get()) {}

		virtual bool runOnBasicBlock(BasicBlock &BB) {
			bool was_changed = false;

			BasicBlock::iterator next_inst;
			BasicBlock::iterator inst_end = BB.end(); --inst_end;
			//LOG_DEBUG() << "BB:\n";
			//LOG_DEBUG() << BB;

			std::set<std::vector<Instruction*> > set_of_chains;

			for(BasicBlock::iterator inst = BB.begin(); inst != inst_end; ++inst) {
				//LOG_DEBUG() << "checking for chain starting at: " << *inst << "\n";

				next_inst = inst;
				++next_inst;

				unsigned int op_type = inst->getOpcode();

				// we only allow integer add and mul for now
				if(!(op_type == Instruction::Add
				  || op_type == Instruction::FAdd
				  || op_type == Instruction::Mul
				  || op_type == Instruction::FMul)
				  ) {
					continue;
				}

				User* first_user = *(inst->use_begin());

				std::vector<Instruction*> inst_chain;

				while(inst->getNumUses() == 1 // have a single use
				  && first_user == next_inst // use is next instruction
				  && next_inst->getOpcode() == op_type // same operation type
				  ) {
					//LOG_DEBUG() << "adding to chain: " << *inst << "\n";
					inst_chain.push_back(inst);

					++inst;
					first_user = *(inst->use_begin());
					++next_inst;
				}

				// we aren't going to bother unless the inst chain is going to
				// be at least 3 insts
				if(inst_chain.size() >= 2) {
					// Need to add inst because it was the "next_inst" checked for
					// the last executed iteration of the while loop.
					inst_chain.push_back(inst);

					LOG_DEBUG() << "found a chain of associative insts (length = " << inst_chain.size() << "):\n";
					for(unsigned i = 0; i < inst_chain.size(); ++i) {
						// TODO: use PRINT_VALUE instead
						LOG_DEBUG() << "\t" << *inst_chain[i] << "\n";
					}

					// add this chain to the set of chains so we can process them later
					set_of_chains.insert(inst_chain);
				}
			}

			for(std::set<std::vector<Instruction*> >::iterator ci = set_of_chains.begin(), ce = set_of_chains.end(); ci != ce; ++ci) {
				LOG_DEBUG() << "processing next chain of insts\n";
				was_changed = true;

				std::vector<Instruction*> chain = *ci;

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
				for(unsigned i = 1; i < curr_chain.size(); ++i) {
					Instruction* curr_inst = dyn_cast<Instruction>(curr_chain[i]);
					if(curr_inst->getOperand(0) == curr_chain[i-1]) {
						// op 1 is the input
						next_chain.push_back(curr_inst->getOperand(1));
					}
					else {
						// op 0 is the input
						next_chain.push_back(curr_inst->getOperand(0));
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
						Instruction* new_inst = BinaryOperator::Create(bin_op->getOpcode(),curr_chain[i],curr_chain[i+1],"adb",chain[1]);
						LOG_DEBUG() << "created new instruction: " << *new_inst << "\n";

						next_chain.push_back(new_inst);
					}
				}

				// Replace uses of the last value in chain with sole member of next_chain
				chain.back()->replaceAllUsesWith(next_chain[0]);
			}

			//log.close();

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

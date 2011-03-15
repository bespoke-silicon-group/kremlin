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

			LOG_DEBUG() << "BB:\n";
			LOG_DEBUG() << BB;
			LOG_DEBUG() << "\n";

			// Determine which nodes can be links in our chains of dep instructions.
			// We also note which ones can ONLY be the end of a chain.
			std::map<Value*,unsigned int> chain_status; // 0 = not in chain, 1 = chain link (or end?), 2 = chain end only

			std::vector<Instruction*> candidate_insts;
			for(BasicBlock::iterator inst = BB.begin(), inst_end = BB.end(); inst != inst_end; ++inst) {
				unsigned int op_type = inst->getOpcode();
				LOG_DEBUG() << "checking for basic criteria: " << *inst << "\n";

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
						LOG_DEBUG() << "\tpotential chain link\n";
						chain_status[inst] = 1;
						candidate_insts.push_back(inst);
					}
					// if it has more than 1 use then it can only be end of chain
					else if(inst->getNumUses() > 1) {
						LOG_DEBUG() << "\tpotential chain end (multi-user)\n";
						chain_status[inst] = 2;
						candidate_insts.push_back(inst);
					}
					// no uses???
					else {
						chain_status[inst] = 0;
					}

					//LOG_DEBUG() << "adding to initial list of candidates: " << *inst << "\n";
				}
				else {
					//LOG_DEBUG() << "does not meet initial candidate criteria: " << *inst << "\n";
					chain_status[inst] = 0;
				}
			}

			if(candidate_insts.empty()) { return false; }


			// We now check the chain link candidates to see which ones have users outside the list
			// of candidates or whose users have a different op type. These candidates
			// become chain_end_only candidates.
			for(BasicBlock::iterator inst = BB.begin(), inst_end = BB.end(); inst != inst_end; ++inst) {
				if(chain_status[inst] != 1) continue;

				Instruction* first_user = dyn_cast<Instruction>(*(inst->use_begin()));

				// If the user isn't in this BB then we know it has to be an end.
				// Likewise, if user isn't in the chain, it can only be an end.
				if(first_user->getParent() != &BB
				    || chain_status[first_user] == 0
					|| first_user->getOpcode() != inst->getOpcode()
				  ) {
					LOG_DEBUG() << "changing status to link end: " << *inst << "\n";
					chain_status[inst] = 2;
				}
			}

			// now we'll try to build the chain of insts
			std::set<std::vector<Instruction*> > set_of_chains;
			std::set<Instruction*> used_insts;

			for(BasicBlock::iterator inst = BB.begin(), inst_end = BB.end(); inst != inst_end; ++inst) {
				if(used_insts.find(inst) != used_insts.end()
					|| chain_status[inst] == 0
				  ) { continue; }

				LOG_DEBUG() << "checking for start of chain at: " << *inst << "\n";

				std::vector<Instruction*> inst_chain;
				Instruction* curr_inst = inst;

				while(chain_status[curr_inst] == 1) {
					inst_chain.push_back(curr_inst);

					curr_inst = dyn_cast<Instruction>(*(curr_inst->use_begin()));
					LOG_DEBUG() << "next link in chain: " << *curr_inst << "\n";
				}

				assert(chain_status[curr_inst] == 2 && "End of chain not marked as end.");

				// don't forget to add the chain end link
				inst_chain.push_back(curr_inst);

				if(inst_chain.size() >= 3) {
					LOG_DEBUG() << "found chain with " << inst_chain.size() << " links:\n";

					set_of_chains.insert(inst_chain);

					for(unsigned i = 0; i < inst_chain.size(); ++i) {
						used_insts.insert(inst_chain[i]);
						LOG_DEBUG() << "\t" << *inst_chain[i] << "\n";
					}
				}
			}

			// Further winnow down list of candidates by getting rid of insts whose 
			// users are not currently in the list of candidates or whose users have
			// a different op type

			// We make a special exception for the last candidate since it is 
			// guaranteed to be the last one in the chain and we don't need to worry 
			// about who uses it.

			/*
			std::set<unsigned int> indices_to_remove;
			for(unsigned i = 0; i < candidate_insts.size()-1; ++i) {
				Instruction* inst = candidate_insts[i];
				User* first_user = *(inst->use_begin());

				// check to make sure
				if(dyn_cast<Instruction>(first_user)->getOpcode() != inst->getOpcode()) {
					LOG_DEBUG() << "removing from list of candidates (REASON: user is of diff op type): " << *inst << "\n";
					indices_to_remove.insert(i);
					continue;
				}

				bool user_in_list = false;
				for(unsigned j = i+1; j < candidate_insts.size(); ++j) {
					if(first_user == candidate_insts[j]) {
						user_in_list = true;
						break;
					}
				}

				if(!user_in_list) {
					LOG_DEBUG() << "removing from list of candidates (REASON: user not in candidates): " << *inst << "\n";
					indices_to_remove.insert(i);
					continue;
				}
			}

			// go ahead and remove those we marked as bad candidates
			std::vector<Instruction*> final_candidate_insts;
			for(unsigned i = 0; i < candidate_insts.size(); ++i) {
				if(indices_to_remove.find(i) == indices_to_remove.end()) {
					final_candidate_insts.push_back(candidate_insts[i]);
				}
			}

			// Only want chains of 3 or longer so we know if we have less than that many candidates
			// there is no use going any further.
			if(final_candidate_insts.size() < 3) { return false; }

			LOG_DEBUG() << "final list of candidates:\n";
			for(unsigned i = 0; i < final_candidate_insts.size(); ++i) {
				LOG_DEBUG() << "\t" << *final_candidate_insts[i] << "\n";
			}
			*/

			//std::set<Instruction*> used_candidates;
			//std::set<std::vector<Instruction*> > set_of_chains;

			// Again, we treat the last candidate specially because we know
			// it cannot start a chain.
			/*
			for(unsigned i = 0; i < final_candidate_insts.size()-1; ++i) {
				Instruction* curr_inst = final_candidate_insts[i];
				LOG_DEBUG() << "checking for chain starting at: " << *curr_inst << "\n";

				if(used_candidates.find(curr_inst) != used_candidates.end()) { continue; }

				unsigned int curr_index = i;

				std::vector<Instruction*> inst_chain;
				inst_chain.push_back(curr_inst);

				// keep following chain, adding them to vector as we go
				User* first_user;
				while(curr_inst != NULL) {
					LOG_DEBUG() << "\tprocessing chain inst: " << *curr_inst << "\n";

					// if we are at the last candidate then we know we can add it
					// to the inst chain.
					// This is the case because we can only have reached the last
					// index from inside this while loop (the outer for loop
					// doesn't visit the last candidate).
					if(curr_index == final_candidate_insts.size()-1) {
						inst_chain.push_back(curr_inst);
						used_candidates.insert(curr_inst);
						curr_inst = NULL;
					}
					else {
						first_user = *(curr_inst->use_begin());

						// Find the next inst in the chain. This is guaranteed
						// to finish since it is not possible to reach this point
						// if curr_inst is the last candidate and also because, by
						// construction, all insts in final_candidate_insts will 
						// have their sole user in final_candidate_insts.
						for(unsigned j = curr_index; j < final_candidate_insts.size(); ++j) {
							if(final_candidate_insts[j] == first_user) {
								inst_chain.push_back(curr_inst);
								used_candidates.insert(curr_inst);

								// update curr_inst for next iter of while loop
								curr_inst = dyn_cast<Instruction>(first_user);
								curr_index = j;

								break;
							}
						}
					}

					assert(false);
				}

				if(inst_chain.size() >= 3) {
					LOG_DEBUG() << "found a chain of associative insts (length = " << inst_chain.size() << "):\n";
					for(unsigned i = 0; i < inst_chain.size(); ++i) {
						// TODO: use PRINT_VALUE instead
						LOG_DEBUG() << "\t" << *inst_chain[i] << "\n";
					}

					// add this chain to the set of chains so we can process them later
					set_of_chains.insert(inst_chain);
				}
			}
			*/

			/*
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
			*/

			for(std::set<std::vector<Instruction*> >::iterator ci = set_of_chains.begin(), ce = set_of_chains.end(); ci != ce; ++ci) {
				LOG_DEBUG() << "processing next chain of insts\n";
				was_changed = true;

				std::vector<Instruction*> chain = *ci;

				for(unsigned i = 0; i < chain.size(); ++i) {
					LOG_DEBUG() << "\t" << *chain[i] << "\n";
				}

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
				LOG_DEBUG() << "external inputs to chain:\n";
				for(unsigned i = 1; i < curr_chain.size(); ++i) {
					Instruction* curr_inst = dyn_cast<Instruction>(curr_chain[i]);
					if(curr_inst->getOperand(0) == curr_chain[i-1]) {
						// op 1 is the input
						next_chain.push_back(curr_inst->getOperand(1));
						LOG_DEBUG() << "\t" << *curr_inst->getOperand(1) << "\n";
					}
					else {
						// op 0 is the input
						next_chain.push_back(curr_inst->getOperand(0));
						LOG_DEBUG() << "\t" << *curr_inst->getOperand(0) << "\n";
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
						LOG_DEBUG() << "created new instruction: " << *new_inst << "\n";

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
			if(was_changed) { LOG_DEBUG() << "modified BB:\n" << BB << "\n"; }

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

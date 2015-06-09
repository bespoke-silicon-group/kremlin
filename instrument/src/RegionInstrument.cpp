#include "llvm/IR/DebugInfo.h"
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IR/User.h>
#include <map>

#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/ReaderWriter.h>

#include <llvm/IR/CFG.h>

#include "foreach.h"
#include "PassLog.h"
#include "LLVMTypes.h"
#include "HashRegionIdGenerator.h"
#include "FuncRegion.h"
#include "LoopRegion.h"
#include "LoopBodyRegion.h"

#include <limits.h>
#include <boost/ptr_container/ptr_set.hpp>
#include <boost/uuid/uuid.hpp>

using namespace llvm;

namespace {

	typedef unsigned long long BasicBlockId;

	static cl::opt<std::string> instFunctionList("instrumented-functions",cl::desc("file that lists which instrumentation functions to insert"),cl::value_desc("filename"),cl::init("__none__"));
	static cl::opt<bool> shouldPrintRegionGraph("output-region-graph",cl::desc("Enables output of region graph file."),cl::Hidden,cl::init(false));
	static cl::opt<std::string> regionGraphFile("region-graph-file",cl::desc("File to put the region graph into"),cl::value_desc("filename"),cl::init("region.graph"));
	static cl::opt<bool> shouldPrintNestingDot("output-nesting-dot",cl::desc("Enables output of nesting graph DOT file."),cl::Hidden,cl::init(false));
	static cl::opt<std::string> nestingDotFile("nesting-dot-file",cl::desc("File to put the nesting graph dot file"),cl::value_desc("filename"),cl::init("nesting.dot"));
	static cl::opt<bool> noMultiExitLoops("no-multi-exit-loop",cl::desc("Disallows profiling of multi-exit loops"),cl::Hidden,cl::init(false));
	static cl::opt<bool> noRecursiveFuncs("no-recursive-funcs",cl::desc("Disallows profiling of recursive functions"),cl::Hidden,cl::init(false));
	static cl::opt<bool> loopBodyRegions("loop-body-regions",cl::desc("Specify that loop body should have its own region ID."),cl::Hidden,cl::init(true));

	struct ModuleLess : public std::less<Module*> {
		bool operator()(Module* m1, Module* m2) {
			return m1->getModuleIdentifier().compare(m2->getModuleIdentifier()) < 0;
		}
	};

	struct RegionInstrument : public ModulePass {
		static char ID;
		static const std::string CPP_ENTRY_FUNC;
		static const std::string CPP_EXIT_FUNC;
        static const std::string CPP_THROW_FUNC_NAME;
        static const std::string CPP_RETHROW_FUNC_NAME;
		static const std::string REGIONS_INFO_FILENAME;

		PassLog& log;

		RegionInstrument() : ModulePass(ID), log(PassLog::get()), region_id_generator(new HashRegionIdGenerator()) {}

		std::set<std::string> definedFunctions;
		std::set<std::string> profilerFunctions;
		boost::scoped_ptr<RegionIdGenerator> region_id_generator;
		boost::ptr_set<Region> regions;

		std::map<RegionId,std::vector<BasicBlockId> > region_id_to_contained_bb_ids;
		std::map<RegionId,BasicBlockId> region_id_to_entry_bb_id;

		std::map<std::string,RegionId> func_name_to_region_id;
		std::ofstream nesting_graph; // DOT version for easy viewing of graph
		std::ofstream region_graph; // file to be used by the parasite

		bool add_initProfiler_func;
		bool add_deinitProfiler_func;
		bool add_logBBVisit_func;
		bool add_logRegionEntry_func;
		bool add_logRegionExit_func;

		// Returns true if this function calls itself directly (i.e. is directly recursive).
		// This doesn't return true for cycles in the call graph involving more than 1 node.
		bool isRecursive(Function* func) {
			for(Value::use_iterator ui = func->use_begin(), uie = func->use_end(); ui != uie; ++ui) {
				Value* use = *ui;
				if(Instruction* inst = dyn_cast<Instruction>(use)) {

					if(inst->getParent()->getParent() == func) {
						log.info() << func->getName() << " is a recursive function\n";
						return true;
					}
				}
			}

			return false;
		}

		// Returns a vector of all the loops described in LoopInfo (including subloops)
		std::vector<Loop*> harvestLoops(LoopInfo& LI,unsigned int& max_depth) {
			// INVARIANT: max_depth == 0
			std::vector<Loop*> loops; // all loops in this function

			// go through all "top level" loops, harvest their subloops and add them to list of all loops
			for(LoopInfo::iterator loop = LI.begin(), loop_end = LI.end(); loop != loop_end; ++loop) {
				std::vector<Loop*> subloops = harvestSubLoops(*loop,1,max_depth); 
				loops.insert(loops.end(),subloops.begin(),subloops.end());
			}

			max_depth *= 2; // mult by 2 to account for loop body regions

			return loops;
		}

		// return a vector containing the input loop and all its subloops (recursing into subloops to get their subloops and so on)
		std::vector<Loop*> harvestSubLoops(Loop* loop, unsigned int curr_depth, unsigned int& max_depth) {
			std::vector<Loop*> loops; // all loops (i.e. this loop + all subloops)
			std::vector<Loop*> tl_subloops = loop->getSubLoops(); // all "top level" subloops

			loops.push_back(loop);

			if(curr_depth > max_depth) max_depth = curr_depth;

			// recursively harvest all subloops of each "top level" subloop and add that to list of all loops
			for(unsigned i = 0; i < tl_subloops.size(); ++i) {
				std::vector<Loop*> subloops = harvestSubLoops(tl_subloops[i],curr_depth+1,max_depth);
				loops.insert(loops.end(),subloops.begin(),subloops.end());
			}

			return loops;
		}

		void replaceAllJumps(BasicBlock *old_dest, BasicBlock *new_dest, Loop *loop, bool preds_in_loop) {
			std::vector<BasicBlock*> preds_to_switch;

			// go through all preds and find which ones we want to change to the new_dest
			for(pred_iterator PI = pred_begin(old_dest), PE = pred_end(old_dest); PI != PE; ++PI) {
				BasicBlock* pred = *PI;

				if((!preds_in_loop && !loop->contains(pred)) || (preds_in_loop && loop->contains(pred))) { 
					preds_to_switch.push_back(pred);
				}
			}

			for(unsigned int i = 0; i < preds_to_switch.size(); ++i) {
				BasicBlock* pred = preds_to_switch[i];

				TerminatorInst* ti = pred->getTerminator();

				// find any successors that point to loop_header and replace them with a pointer to preheader
				if(BranchInst* bi = dyn_cast<BranchInst>(ti)) {
					for(unsigned i = 0; i < bi->getNumSuccessors(); ++i) {
						if(bi->getSuccessor(i) == old_dest) {
							bi->setSuccessor(i,new_dest);
						}
					}
				}
				else if(SwitchInst* swi = dyn_cast<SwitchInst>(ti)) {
					for(unsigned i = 0; i < swi->getNumSuccessors(); ++i) {
						if(swi->getSuccessor(i) == old_dest) {
							swi->setSuccessor(i,new_dest);
						}
					}
				}
				else if(InvokeInst* ii = dyn_cast<InvokeInst>(ti)) {
					if(ii->getNormalDest() == old_dest)
						ii->setNormalDest(new_dest);

					if(ii->getUnwindDest() == old_dest)
						ii->setUnwindDest(new_dest);
				}
				else
				{
					log.debug() << "Terminator inst: " << *ti;
					assert(0 && "terminator for pred must end with branch, switch, or invoke");
				}
			}
		}

		// returns true if possible_pred is a pred of bb, false otherwise
		bool isPredecessor(BasicBlock* bb, BasicBlock* possible_pred) {
			for(pred_iterator PI = pred_begin(bb), PE = pred_end(bb); PI != PE; ++PI) {
				if(possible_pred == *PI)
					return true;
			}

			// didn't find anything so it must not be a pred
			return false;
		}

		void promotePHIsToOtherBlock(BasicBlock *src_bb, BasicBlock *dest_bb) {
			// loop through all PHIs in the src_bb
			for(BasicBlock::iterator phi = src_bb->begin(), phi_end = src_bb->getFirstNonPHI(); phi != phi_end; ++phi) {
				PHINode *orig_phi = dyn_cast<PHINode>(phi);

				// create new phi in dest_bb
				PHINode *promoted_phi = PHINode::Create(orig_phi->getType(), 0, orig_phi->getName() + ".promoted", dest_bb->getFirstNonPHI());

				// vector of ints representing the incoming values of the orig phi that we want to remove
				// we need this because we don't want to delete from something we are iterating through
				std::vector<unsigned int> to_remove_from_orig;

				// Loop through all values of original phi and find any coming from a block that isn't a pred of src_bb (i.e. it was moved to dest_bb)
				// We are then going to add this value to the promoted_phi.
				for(unsigned i = 0; i < orig_phi->getNumIncomingValues(); ++i) {
					BasicBlock *incoming_block = orig_phi->getIncomingBlock(i);

					if(!isPredecessor(src_bb,incoming_block)) {
						// since we will be deleting indices, we need to scale based on how many we will have deleted by the time we reach this one
						to_remove_from_orig.push_back(i-to_remove_from_orig.size());

						promoted_phi->addIncoming(orig_phi->getIncomingValue(i),incoming_block);
					}
				}

				// if there wasn't anything to promote, erase the new phi and don't do anything else
				if(promoted_phi->getNumIncomingValues() == 0) {
					promoted_phi->eraseFromParent();

					log.debug() << "No values to promote from " << src_bb->getName() << " to " << dest_bb->getName() << "\n";
					return;
				}

				// now remove those incoming values we noted
				for(unsigned i = 0; i < to_remove_from_orig.size(); ++i) {
					orig_phi->removeIncomingValue(to_remove_from_orig[i],false);
				}

				// if there is only 1 incoming value for the new PHI, we can use that value directly in old_phi and ditch the new promoted phi
				// TODO: didn't we see a case where there was a duplicate entry in the PHI so it looked like there were 2 but 1? should we
				//	check for that here?
				if(promoted_phi->getNumIncomingValues() == 1) {
					// make sure we say it is coming via the dest_bb though
					orig_phi->addIncoming(promoted_phi->getIncomingValue(0),dest_bb);

					promoted_phi->eraseFromParent();
				}

				// we now add promoted_phi as a value to the orig_phi; in a way we are piping the intended value through promoted_phi
				// and into the orig_phi
				else {
					orig_phi->addIncoming(promoted_phi,dest_bb);
				}
			}
		}

		void instrumentLoop(Loop* loop, LoopInfo& LI, RegionId region_id, RegionId body_region_id, Function* logRegionEntry_func, Function* logRegionExit_func, Function* logLandingPad_func, std::map<Loop*, std::vector<BasicBlock*> >& loop_to_landing_pads_map) {
			BasicBlock *loop_header = loop->getHeader();
			LLVMTypes types(loop_header->getContext());

			// First, we are going to check that this loop has only 1 exit bb. If it has multiple exit-bbs, our outliner won't
			//  be able to handle it.
			SmallVector<BasicBlock*,16> exit_bbs;
			loop->getExitBlocks(exit_bbs);

			// need to make sure the list of exit_bbs is uniquified so we convert the vector to a set
			std::set<BasicBlock*> exit_bbs_set;
			exit_bbs_set.insert(exit_bbs.begin(),exit_bbs.end());

			// make sure we aren't trying to instrument a multi-exit loop if we don't want to
			if(noMultiExitLoops && exit_bbs_set.size() > 1) {
				log.info() << "not instrumenting multi-exit loop (region_id: " << region_id << ", header = " << loop_header->getName() << ")\n";
				return;
			}

			// create a pre-header BB where we will insert a call to
			// _KEnterRegion()
			BasicBlock* preheader = BasicBlock::Create(loop_header->getContext(), loop_header->getName() + ".preheader", loop_header->getParent(), loop_header);

#if 0
			if(loop->getParentLoop()) {
				loop->getParentLoop()->addBasicBlockToLoop(preheader,LI.getBase());
			}
#endif

			// go through all preds of loop_header that are NOT in loop and change them to branch to preheader
			replaceAllJumps(loop_header,preheader,loop,false);

			// create jump from preheader to loop_header
			BranchInst::Create(loop_header,preheader);

			promotePHIsToOtherBlock(loop_header, preheader);

			// scan through loop_header to see if we have any calls to
			// _KEnterRegion()... if so we move those to the preheader
			std::vector<CallInst*> to_move;

			for(BasicBlock::iterator inst = loop_header->getFirstNonPHI(), inst_end = loop_header->end(); inst != inst_end; ++inst) {
				if(CallInst* ci = dyn_cast<CallInst>(inst)) {
					if(ci->getCalledFunction() && ci->getCalledFunction()->getName() == "_KEnterRegion") {
						to_move.push_back(ci);
					}
				}
			}

			for(unsigned i = 0; i < to_move.size(); ++i) {
				to_move[i]->moveBefore(preheader->getTerminator());
			}

			std::vector<Value*> op_args;

			// set up args for call to _KEnterRegion(region_id,region_type) in the preheader
			op_args.push_back(ConstantInt::get(types.i64(),region_id));
			op_args.push_back(ConstantInt::get(types.i32(),Region::REGION_TYPE_LOOP)); // 2nd arg = 1 means that this is a loop region

			// insert call right before we jump to the actual header
			ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args);
			CallInst::Create(logRegionEntry_func, *aref, "", preheader->getTerminator());
			delete aref;
			aref = NULL;

#if 0
			op_args.clear();

			// set up args for call to _KEnter/ExitRegion(region_id,region_type) for loop region
			op_args.push_back(ConstantInt::get(types.i64(),region_id));
			op_args.push_back(ConstantInt::get(types.i32(),1));
#endif

			std::vector<Value*> op_args_loop_body;

			// set up args for call to _KEnter/ExitRegion(region_id,region_type) for loop body regions
			//std::string loop_body_name = loop->getHeader()->getName().str() + "_body";
			op_args_loop_body.push_back(ConstantInt::get(types.i64(),body_region_id));
			op_args_loop_body.push_back(ConstantInt::get(types.i32(),Region::REGION_TYPE_LOOP_BODY)); // loop body regions have type ID = 2

			SmallVector<BasicBlock*,16> exiting_bbs;
			loop->getExitingBlocks(exiting_bbs);

			// Instrument loop regions
			for(unsigned i = 0; i < exiting_bbs.size(); ++i) {
				BasicBlock* exiting_bb = exiting_bbs[i];

				// find which BB we are going to exit to
				std::set<BasicBlock*> target_bbs;

				for(succ_iterator SI = succ_begin(exiting_bb), SE = succ_end(exiting_bb); SI != SE; ++SI) {
					BasicBlock* succ = *SI;

					if(!loop->contains(succ) 
					  ) {
						//log.debug() << "found target_bb outside of loop: " << succ->getName() << "\n";
						target_bbs.insert(succ);
					}
				}

				assert(!target_bbs.empty() && "no target bb outside of loop found");

				log.debug() << exiting_bb->getName() << " has " << target_bbs.size() << " possible loop exit points\n";

				int tar_no = 0;
				for(std::set<BasicBlock*>::iterator tar_it = target_bbs.begin(), tar_end = target_bbs.end(); tar_it != tar_end; ++tar_it, ++tar_no) {
					BasicBlock* target = *tar_it;

					if (target->isLandingPad()) {
						log.debug() << "target #" << tar_no << " (" << target->getName() << ") is a landing pad. Skipping.\n";
						continue;
					}

					log.debug() << "\ttarget #" << tar_no << ": " << target->getName() << "\n";

					// We now create a basic block that will act as the
					// "pre-exit" for the loop and contain a call to
					// _KExitRegion().
					BasicBlock* pre_exit = BasicBlock::Create(loop_header->getContext(), exiting_bb->getName() + ".pre_exit." + target->getName(), loop_header->getParent(), target);

					// jump to target from pre_exit
					BranchInst::Create(target,pre_exit);

					// If we are instrumenting loop bodies, we need a call to
					// _KExitRegion for the loop body region before exiting the loop region.
					// We don't want to insert the call if the exiting_bb is the loop header because this implies the header is part of the
					// loop body. If we did insert _KExitRegion here and the loop header isn't part of the body, there is a path that will go through 
					// _KExitRegion for the loop body but not _KEnterRegion for the body.
					if(loopBodyRegions 
					  // XXX: Later we assume switch implies header is part of
					  // body so we have to insert _KExitRegion even if exiting_bb is a header.
					  && (exiting_bb != loop_header || isa<SwitchInst>(loop_header->getTerminator())) 
					  ) {
						ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args_loop_body);
						CallInst::Create(logRegionExit_func, *aref, "", pre_exit->getTerminator());
						delete aref;
					}

					// insert call to _KExitRegion() for loop region right before we leave pre_exit
					ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args);
					CallInst::Create(logRegionExit_func, *aref, "", pre_exit->getTerminator());
					delete aref;

					// in the target BB, replace all references to exiting_bb with a reference to pre_exit (this will be phi nodes only)
					/*
					for(BasicBlock::iterator phi = target->begin(), phi_end = target->getFirstNonPHI(); phi != phi_end; ++phi) {
						phi->replaceUsesOfWith(exiting_bb, pre_exit);
					}
					*/

					// In the exiting block, replace references to target with a reference to pre_exit.
					// This should only occur in the terminator inst.
					exiting_bb->getTerminator()->replaceUsesOfWith(target,pre_exit);

					promotePHIsToOtherBlock(target, pre_exit);

				}
			}

			// Instrument loop body regions
			if(loopBodyRegions) {
				// if the loop header is part of the body (i.e. a do-while
				// loop) then call to _KEnterRegion goes in loop header, otherwise
				// the call goes to the successor BB that is in the loop (i.e. first block in loop body).
				// If either of the following two conditions is true, we consider the loop header to NOT be part of the body:
				//	  1) ends with a switch statement
				//    2) one of the header's successors is not in the loop
				
				ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args_loop_body);
				CallInst* ci = CallInst::Create(logRegionEntry_func, *aref, "");
				delete aref;

				if(!isa<BranchInst>(loop_header->getTerminator()) // not a branch... must be a switch?
				  || loop->getBlocks().size() == 1 // if this is a single-BB loop then header MUST be in the body
				  ) { 
					ci->insertBefore(loop_header->getFirstNonPHI());
				}
				else {
					// this is a branch so we'll see if any of the successors aren't in this loop
					bool succ_not_in_loop = false;
					BasicBlock* first_loop_bb = NULL;

					// there should only be 2 successors (more than 2 requires switch, 0 is impossible because we have simplified the CFG)
					for(succ_iterator SI = succ_begin(loop_header), SE = succ_end(loop_header); SI != SE; ++SI) {
						if(!loop->contains(*SI)) {
							succ_not_in_loop = true;
						}
						else {
							first_loop_bb = *SI;
						}
					}

					if(succ_not_in_loop) { // a success was not in loop so the _KEnterRegion goes in first BB in the loop body
						ci->insertBefore(first_loop_bb->getFirstNonPHI());
					}
					else { // all successors are in the loop so header is part of loop (insert _KEnterRegion there)
						ci->insertBefore(loop_header->getFirstNonPHI());
					}
				}

				// If this is a single-BB loop then we need to insert the
				// _KExitRegion at the end of the header.
				// Otherwise, we want a single BB where all "continue" edges will go to (so we can insert call to 
				// _KExitRegion for loop body region there).
				// We don't want to create the extra BB in the single-BB loop case because it wouldn't have anything
				// in it.
				aref = new ArrayRef<Value*>(op_args_loop_body);
				ci = CallInst::Create(logRegionExit_func, *aref, ""); // defer placement for now
				delete aref;

				if(loop->getBlocks().size() == 1) {
					ci->insertBefore(loop_header->getTerminator());
				}
				else {
					BasicBlock* cont_bb = BasicBlock::Create(loop_header->getContext(), loop_header->getName() + ".cont", loop_header->getParent(), loop_header);

					BranchInst::Create(loop_header,cont_bb); // create jump from cont_bb to loop_header

					//CallInst::Create(logRegionExit_func,
					//op_args_loop_body.begin(), op_args_loop_body.end(), "",
					//cont_bb->getTerminator()); // now the call to
					//_KExitRegion
					ci->insertBefore(cont_bb->getTerminator());

					// replace all jumps to loop_header that are in the loop to cont_bb
					replaceAllJumps(loop_header,cont_bb,loop,true);

					promotePHIsToOtherBlock(loop_header, cont_bb);
				}
			}

			// Insert call to _KLandingPad in all landing pads in this loop.
			std::vector<BasicBlock*> landing_pads = loop_to_landing_pads_map[loop];
			for (unsigned i = 0; i < landing_pads.size(); ++i) {
				op_args.clear();
				if (loopBodyRegions) {
					op_args.push_back(ConstantInt::get(types.i64(),body_region_id));
					op_args.push_back(ConstantInt::get(types.i32(),Region::REGION_TYPE_LOOP_BODY));
				}
				else {
					op_args.push_back(ConstantInt::get(types.i64(),region_id));
					op_args.push_back(ConstantInt::get(types.i32(),Region::REGION_TYPE_LOOP));
				}
				aref = new ArrayRef<Value*>(op_args);
				CallInst::Create(logLandingPad_func, *aref, "", landing_pads[i]->getFirstInsertionPt());
				delete aref;
				op_args.clear();
			}


#if 0
			// go through all exit blocks and insert a BB before them that
			// loop nodes jump to (instead of exit bb) and which makes a call
			// to _KExitRegion
			for(std::set<BasicBlock*>::iterator exit_bb_it = exit_bbs_set.begin(), exit_bb_end = exit_bbs_set.end(); exit_bb_it != exit_bb_end; ++exit_bb_it) {
				BasicBlock* exit_bb = *exit_bb_it;

				// We now create a basic block that will act as the single
				// exit for the loop and contain a call to _KExitRegion().
				BasicBlock* loop_exit = BasicBlock::Create(exit_bb->getName() + ".pre_exit", loop_header->getParent(),exit_bb);

				// jump from loop_exit to the exit_bb
				BranchInst::Create(exit_bb,loop_exit);

				// insert call to _KExitRegion() right before we leave loop_exit
				CallInst* ci = CallInst::Create(logRegionExit_func, op_args.begin(), op_args.end(), "", loop_exit->getTerminator());

				// if we are instrumenting loop bodies, we need a call to
				// _KExitRegion for the loop body region before exiting the loop region
				if(loopBodyRegions) {
					CallInst::Create(logRegionExit_func, op_args_loop_body.begin(), op_args_loop_body.end(), "", ci);
				}

				// go through all preds of exit_bb that are in loop and change them to branch to loop_exit
				replaceAllJumps(exit_bb,loop_exit,loop,true);

				// fix the PHIs in exit_bb so they don't refer to any blocks that now go to loop_exit instead
				promotePHIsToOtherBlock(exit_bb, loop_exit);
			}
#endif
		}

		unsigned int getNumInsts(BasicBlock* bb) {
			unsigned int num_insts = 0;

			for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; ++i) {
				if(isa<BinaryOperator>(i) || isa<CmpInst>(i))
					num_insts++;
			}

			return num_insts;
		}

		unsigned int getNumPreds(BasicBlock* bb) {
			unsigned int num_preds = 0;

			for(pred_iterator PI = pred_begin(bb), PE = pred_begin(bb); PI != PE; ++PI)
				num_preds++;

			return num_preds;
		}

		// returns true if CallInst is a call that we can inline. If it is inlinable it implies:
		// 1) It is not a function pointer
		// 2) It is either a region instrument function (e.g. _KBasicBlock) or function whose body we have the bitcode for
		bool isInlinableCall(CallInst* ci) {
			// if this is a function pointer then we can't inline
			if(ci->getCalledFunction() == NULL)
				return false;

			// if this isn't in our list of defined or profiler functions then we can't inline it
			else if(definedFunctions.find(ci->getCalledFunction()->getName()) == definedFunctions.end()
			  && profilerFunctions.find(ci->getCalledFunction()->getName()) == profilerFunctions.end()
			  )
				return false;

			else
				return true;
		}
		
		bool callsNonInlinableFunc(BasicBlock* bb) {
			for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; ++i) {
				if(isa<CallInst>(i)) {
					CallInst* ci = dyn_cast<CallInst>(i);

					if(!isInlinableCall(ci)) return true;
				}
			}

			// didn't find any non-inliable calls so we are good to go
			return false;
		}

		void printFunctionNesting(BasicBlock* bb, RegionId func_region_id, LoopInfo& LI, std::map<std::string,RegionId>& loop_header_name_to_region_id) {
			// go through all instructions in this BB
			for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; ++i) {
				// if the inst is a callinst that is inlinable, we should add it to the nesting and region graphs
				if(CallInst* ci = dyn_cast<CallInst>(i)) {

					if(isInlinableCall(ci) // is inlinable function
					  && profilerFunctions.find(ci->getCalledFunction()->getName()) == profilerFunctions.end() // not one of the profile funcs
					  ) {
						RegionId region_id = func_region_id;

						// if this is contained within a loop, then print the region id of that loop instead of the region_id of the func
						if(Loop* loop = LI.getLoopFor(bb)) { // getLoopFor returns the inner most loop that bb is in (or NULL if not in a loop)
							region_id = loop_header_name_to_region_id[loop->getHeader()->getName()];
						}

						nesting_graph << "\t" << region_id << " -> " << func_name_to_region_id[ci->getCalledFunction()->getName()] << ";\n";
						if (shouldPrintRegionGraph) region_graph << region_id << " " << func_name_to_region_id[ci->getCalledFunction()->getName()] << "\n";
					}
				}
			}
		}

		virtual bool runOnModule(Module &m) {
			LLVMTypes types(m.getContext());

			if(noMultiExitLoops) { log.info() << "Multi-exit loops will not be instrumented.\n"; }

			if(noRecursiveFuncs) { log.info() << "Recursive funcs will not be instrumented\n"; }
			if(loopBodyRegions) { log.info() << "Loop bodies will be given their own region id\n"; }

			BasicBlockId bb_id = 1; // unique id for each BB in the program

			std::ifstream file;
			std::string line;
			std::string error;

			// if we specified a file to give us which functions to insert, do that now
			if(instFunctionList != "__none__") {
				file.open(instFunctionList.c_str());
				log.debug() << "opening file with list of instrumentation funcs to add: " << instFunctionList << "\n";

				if(!file.is_open()) {
					log.fatal() << "Could not open instrumented function list file: " << instFunctionList << "\nAborting ...\n";
					return false;
				}

				add_initProfiler_func = false;
				add_deinitProfiler_func = false;
				add_logBBVisit_func = false;
				add_logRegionEntry_func = false;
				add_logRegionExit_func = false;

				// read in all the modules
				while(!file.eof()) {
					getline(file,line);
					log.debug() << "specified instrumentation for: " << line << "\n";

					if(line == "_KInit") add_initProfiler_func = true;
					else if(line == "_KDeinit") add_deinitProfiler_func = true;
					else if(line == "_KBasicBlock") add_logBBVisit_func = true;
					else if(line == "_KEnterRegion") add_logRegionEntry_func = true;
					else if(line == "_KExitRegion") add_logRegionExit_func = true;
					else if(line != "") log.warn() << "unknown instrumentation function: " << line << "\n";
				}

				// Certain groups of functions must either all be added or none be added.
				// We check here to make sure those requirements are met.
				if(add_initProfiler_func != add_deinitProfiler_func) {
					log.error() << "either both or neither of _KInit and _KDeinit must be specified\n";
					return false;
				}
				else if(add_logRegionEntry_func != add_logRegionExit_func) {
					log.error() << "either both or neither of _KEnterRegion and _KExitRegion must be specified\n";
					return false;
				}

				file.close();
			}

			else { // otherwise, we assume we do full instrumenting
				add_initProfiler_func = true;
				add_deinitProfiler_func = true;
				add_logBBVisit_func = true;
				add_logRegionEntry_func = true;
				add_logRegionExit_func = true;
			}

            add_logBBVisit_func = false;
			// open file we will write region and nesting graphs too
			// TODO: make sure we incorporate loop body regions now

			if (shouldPrintRegionGraph) region_graph.open(regionGraphFile.c_str());
			if (shouldPrintNestingDot) {
				nesting_graph.open(nestingDotFile.c_str());
				nesting_graph << "digraph nest {\n";
			}


			// loop through each function and see if it is a definition of declaration
			foreach(Function& func, m) {

				// if not a declaration, this is a definition so we add to the map
				if(!func.isDeclaration() 
				  && func.getLinkage() != GlobalValue::AvailableExternallyLinkage
				  && !func.isVarArg()
				  && (!noRecursiveFuncs || !isRecursive(&func)) 
				  ) {
					// if this function name already exists (b/c of static functions in different modules) then we
					// rename it to be unique. The renaming is just the original name with "__<modulename>" tacked
					// onto the end
					if(definedFunctions.find(func.getName()) != definedFunctions.end()) {
						std::string new_func_name = func.getName().str() + "__";

						std::string mod_name = m.getModuleIdentifier();
						
						// strip off any leading dir name structure from module identifier if it exists
						if(mod_name.find("/") != std::string::npos) { mod_name = mod_name.substr(mod_name.rfind("/")+1); }

						// we want the actual module name, not the module.blah.boink.etc.bc so we strip off everything after the first "."
						mod_name = mod_name.substr(0,mod_name.find("."));

						new_func_name += mod_name;
						log.warn() << "Found duplicate function name " << func.getName() << ". Renaming this instance to " << new_func_name << "\n";
						func.setName(new_func_name);
					}

					RegionId region_id = (*region_id_generator)(func.getName());
					FuncRegion* r = new FuncRegion(region_id, &func);
					regions.insert(r);


					log.debug() << "new region: " << *r << "\n";

					log.debug() << "adding " << func.getName() << " (in module: " << m.getModuleIdentifier() << ") to list of defined functions (region_id: " << region_id << ")\n";
					definedFunctions.insert(func.getName());

					// we give region_ids to all functions now so we can build the nesting graph as we go along

					func_name_to_region_id[func.getName()] = region_id;
					log.debug() << "assigning function " << func.getName() << " id " << region_id << "\n";

					if (shouldPrintNestingDot) nesting_graph << "\t" << region_id << " [label=\"" << func.getName().str() << "\"];\n";

				}
			}

			log.debug() << "found " << regions.size() << " func regions\n";

			// XXX: probably a better way to do this...
			// add instrumentation functions to list of defined functions so callsNonInlinableFunc() doesn't get mixed up
			if(add_initProfiler_func)
				profilerFunctions.insert("_KInit");
			if(add_deinitProfiler_func)
				profilerFunctions.insert("_KDeinit");
			if(add_logBBVisit_func)
				profilerFunctions.insert("_KBasicBlock");
			if(add_logRegionEntry_func)
				profilerFunctions.insert("_KEnterRegion");
			if(add_logRegionExit_func)
				profilerFunctions.insert("_KExitRegion");

			profilerFunctions.insert("_KLandingPad");

			log.info() << "Inserting the following instrumentation calls:\n";
			foreach(const std::string& func_name, profilerFunctions)
				log.info() << func_name << "\n";

			DebugInfoFinder debug_info_finder;
			debug_info_finder.processModule(m);

			log.debug() << "Debug information:\n";

			for(DebugInfoFinder::compile_unit_iterator it = debug_info_finder.compile_units().begin(), end = debug_info_finder.compile_units().end(); it != end; it++)
				(*it)->dump();

			for(DebugInfoFinder::subprogram_iterator it = debug_info_finder.subprograms().begin(), end = debug_info_finder.subprograms().end(); it != end; it++)
				(*it)->dump();

			for(DebugInfoFinder::global_variable_iterator it = debug_info_finder.global_variables().begin(), end = debug_info_finder.global_variables().end(); it != end; it++)
				(*it)->dump();

			for(DebugInfoFinder::type_iterator it = debug_info_finder.types().begin(), end = debug_info_finder.types().end(); it != end; it++)
				(*it)->dump();

			
			instrumentModule(m,bb_id);

			if (shouldPrintNestingDot) {
				nesting_graph << "}\n";
				nesting_graph.close();
			}
			if (shouldPrintRegionGraph) region_graph.close();

			return true;
		} // end runOnModule(...)

		bool isCppExitFunc(Function* func)
		{
			std::string name = func->getName().str();
			if(name.length() < CPP_EXIT_FUNC.length())
				return false;
			std::string prefix = name.substr(0, CPP_EXIT_FUNC.length());
			std::string numbers = name.substr(CPP_EXIT_FUNC.length(), name.length() - CPP_EXIT_FUNC.length());

			//check that the numbers were really numbers
			double d;
			std::istringstream iss(numbers);
			iss >> d;
			bool was_number = iss.eof() && !iss.fail();

			return prefix == CPP_EXIT_FUNC && was_number;
		}

        bool isCppThrowFunc(Function* func)
        {
            return func->getName() == CPP_THROW_FUNC_NAME ||
                func->getName() == CPP_RETHROW_FUNC_NAME;
        }

		// INVARIANT: exactly one call to _KPrepRTable in entry BB of func
		void updateRegTableSetup(Function* func, unsigned int max_depth) {
			LLVMTypes types(func->getContext());

			BasicBlock* entry_bb = func->begin();

			// scan through insts to find call to _KPrepRTable
			// note: entry bb shouldn't have any PHI nodes so don't bother
			// with the getFirstNonPHI() function
			for(BasicBlock::iterator inst = entry_bb->begin(), inst_end = entry_bb->end(); inst != inst_end; ++inst) {
				if(CallInst* ci = dyn_cast<CallInst>(inst)) {
					Function* called_func = ci->getCalledFunction();
					// check to see if this is call to _KPrepRTable
					if(called_func
						&& called_func->hasName()
						&& called_func->getName().compare("_KPrepRTable") == 0
					  ) {
						// TODO: sanity check to make sure arg 2 is constant 0
						ci->setArgOperand(1,ConstantInt::get(types.i32(),max_depth));
						break;
					}
				}
			}
		}

		void instrumentModule(Module &m, BasicBlockId &bb_id) {
			LLVMTypes types(m.getContext());

			std::string mod_name = m.getModuleIdentifier();

			std::set<CallInst*> instrumentation_calls;

			std::vector<Type*> args;

			// these will be our instrumentation functions
			Function* initProfiler_func = NULL;
			Function* deinitProfiler_func = NULL;
			Function* logBBVisit_func = NULL;
			Function* logRegionEntry_func = NULL;
			Function* logRegionExit_func = NULL;
			Function* logLandingPad_func = NULL;

			if(add_initProfiler_func) {
				initProfiler_func = cast<Function>(m.getOrInsertFunction("_KInit", FunctionType::get(types.voidTy(), false)));
				deinitProfiler_func = cast<Function>(m.getOrInsertFunction("_KDeinit", FunctionType::get(types.voidTy(), false)));
			}

			args.push_back(types.i64()); // unique ID (bb_id for _KBasicBlock, region_id for _KEnter/ExitRegion)

			if(add_logBBVisit_func)
			{
				ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
				Constant* func_as_const = m.getOrInsertFunction("_KBasicBlock", FunctionType::get(types.voidTy(), *aref, false));
				delete aref;
				log.info() << "func as const: " << *func_as_const << "\n"; // XXX: expensive
				logBBVisit_func = cast<Function>(func_as_const);

				//logBBVisit_func =
				//cast<Function>(m.getOrInsertFunction("_KBasicBlock", FunctionType::get(types.voidTy(), args, false)));
			}

			args.push_back(types.i32()); // 2nd arg is the region type

			ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
			if(add_logRegionEntry_func) {
				logRegionEntry_func = cast<Function>(m.getOrInsertFunction("_KEnterRegion", FunctionType::get(types.voidTy(), *aref, false)));
				logRegionExit_func = cast<Function>(m.getOrInsertFunction("_KExitRegion", FunctionType::get(types.voidTy(), *aref, false)));
			}

			logLandingPad_func = cast<Function>(m.getOrInsertFunction("_KLandingPad", FunctionType::get(types.voidTy(), *aref, false)));
			delete aref;

			args.clear();


			std::vector<Value*> op_args;

			foreach(Function& func, m) {

				if( 	func.isDeclaration() // can't do anything with declarations
					|| func.isVarArg() // currently don't handle vararg functions
					|| (noRecursiveFuncs && isRecursive(&func)) // make sure we don't try a recursive func if we say not to
					//or if the functions are the C++ startup and cleanup functions, then skip them too
					|| (func.hasSection() && 
						StringRef(func.getSection()) == ".text.startup")
					|| (func.hasSection() && 
						StringRef(func.getSection()) == ".text.exit")
				  )
				{
					continue;
				}

				//log.debug() << "instrumenting function " << func.getName() << "\n";

				// NOTE: Ordering of the instrumentation function insertion is
				// important. _KBasicBlock() should come first, so it will end
				//  up being the last instrumentation function called in a BB.
				//  Next, _KEnterRegion() for loops should be inserted so
				//  that they come after a possible call to _KEnterRegion() for a loop. We need this so that a freq for regions is
				//  correctly identified (i.e. the function will be the context from which a loop is entered)

				// TODO: keep list of functions we have seen calls to so far so that we dont' have multiple edges in our region graph
				//  for the case where a function_A calls function_B multiple times

				LoopInfo &LI = getAnalysis<LoopInfo>(func);

				RegionId func_region_id = func_name_to_region_id[func.getName()];
				log.debug() << "region name to id: " << func.getName() << " is " << func_region_id << "\n";

				// get all the loops that exist in this function
				unsigned int max_depth = 0;
				std::vector<Loop*> function_loops = harvestLoops(LI,max_depth);
				//std::sort(function_loops.begin(), function_loops.end());

				// use max_depth info to update _KPrepRTable
				updateRegTableSetup(&func,max_depth);

				log.debug() << "found " << function_loops.size() << " loops in function " << func.getName() << "\n";

				// assign all the loops a region id
				std::map<std::string,RegionId> loop_header_name_to_region_id;
				std::vector<RegionId> loop_body_region_ids;

				std::set<BasicBlock*> loop_headers;

				for(unsigned i = 0; i < function_loops.size(); ++i) {
					Loop* loop = function_loops[i];
					BasicBlock* loop_header = function_loops[i]->getHeader();

					// insert this into a list of loop headers (to look up later for region_id_to_entry_bb_id mapping
					if(find(loop_headers.begin(), loop_headers.end(), loop_header) != loop_headers.end())
						continue;
					loop_headers.insert(loop_header);

					std::string loop_name = func.getName().str() + "_" + loop_header->getName().str();
					std::string loop_header_name = loop_name + "_header";

					RegionId region_id = (*region_id_generator)(loop_header_name);
					loop_header_name_to_region_id[loop_header->getName()] = region_id;
					regions.insert(new LoopRegion(region_id, loop));

					log.debug() << "assigning id = " << region_id << " to loop with the following BBs:\n\t";
					
					for(std::vector<BasicBlock*>::const_iterator block = function_loops[i]->getBlocks().begin(), block_end = function_loops[i]->getBlocks().end(); block != block_end; block++)
					{
						log.plain() << (*block)->getName() << " ";
					}
					log.plain() << "\n";

					std::string loop_body_name = loop_name + "_body";
					region_id = (*region_id_generator)(loop_body_name);

					loop_body_region_ids.push_back(region_id);
					regions.insert(new LoopBodyRegion(region_id, loop));
					log.debug() << "assigning id = " << region_id << " to body of loop\n";
				}

				// Create mapping between loop and landing pads they contain.
				// For any landing pads that aren't in loops, we insert a call
				// to _KLandingPad now, using the function's region id/type.
				std::map<Loop*, std::vector<BasicBlock*> > loop_to_landing_pads_map;
				foreach (BasicBlock& bb, func) {
					if (!bb.isLandingPad()) continue;
					Loop* l = LI.getLoopFor(&bb);

					if (l != NULL) {
						loop_to_landing_pads_map[l].push_back(&bb);
					}
					else {
						op_args.push_back(ConstantInt::get(types.i64(),func_region_id));
						op_args.push_back(ConstantInt::get(types.i32(),Region::REGION_TYPE_FUNC));
						ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args);
						CallInst::Create(logLandingPad_func, *aref, "", bb.getFirstInsertionPt());
						delete aref;
						op_args.clear();
					}
				}

				// Insert call to logVisitBB(bb_id) at the beginning of each bb.
				// We also look to see if the BB ends with a return inst, in which case we need to insert a call
				//   to _KExitRegion() for this function
				for(Function::iterator bb = func.begin(), bb_begin = func.begin(), bb_end = func.end(); bb != bb_end; ++bb) {
					// if this is the entry to the function, we note the bb_id (for printing out later)
					if(bb == bb_begin) {
						region_id_to_entry_bb_id[func_region_id] = bb_id;
					}

					// if this is a loop header, we look up the region_id of the loop it is assoc. with and map bb_id to that region_id's entry
					if(loop_headers.find(bb) != loop_headers.end()) {
						region_id_to_entry_bb_id[loop_header_name_to_region_id[bb->getName()]] = bb_id;
					}

					// add "bb_id.Nx" to the end of the BB name so we can easily identify after inlining
					std::stringstream ss;
					ss << bb_id << "x";

					// need to update loop_header_to_region_id if this is actually a loop header
					if(loop_header_name_to_region_id[bb->getName()] != 0) {
						loop_header_name_to_region_id[bb->getName().str()] = loop_header_name_to_region_id[bb->getName().str()];
					}

					if(add_logBBVisit_func) {
						op_args.push_back(ConstantInt::get(types.i64(),bb_id));

						ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args);
						CallInst::Create(logBBVisit_func, *aref, "", bb->getFirstNonPHI());
						delete aref;
						op_args.clear();
					}

					// add this bb_id to the vector of bb_ids that are associated with this function
					region_id_to_contained_bb_ids[func_region_id].push_back(bb_id);

					// check to see which loops this BB is in and add it to the vector of bb_ids for those loops
					for(unsigned i = 0; i < function_loops.size(); ++i) {
						if(function_loops[i]->contains(bb)) {
							region_id_to_contained_bb_ids[loop_header_name_to_region_id[ function_loops[i]->getHeader()->getName() ]].push_back(bb_id);
						}
					}

					// for all func calls from this BB, add in the nesting info between this region and the region of that function
					printFunctionNesting(bb,func_region_id,LI,loop_header_name_to_region_id);

					if(succ_begin(bb) == succ_end(bb)) {

						BasicBlock::iterator insert_before = bb->getTerminator();
                        CallInst* non_returning_call = NULL;

						bool is_exit_point = false;

						// If the terminator is unreachable, that means we need to wrap up before the instruction before that.
						// That should be a call to some non-returning function (i.e. exit())
						if(isa<UnreachableInst>(bb->getTerminator())) {

							// Try call before terminator as the end.
							BasicBlock::InstListType& insts = bb->getInstList();
							if(insts.size() > 1) {

								BasicBlock::InstListType::reverse_iterator inst_before_term_it = insts.rbegin();
								inst_before_term_it++;
								Instruction* inst_before_term = &*inst_before_term_it;

								if(isa<CallInst>(inst_before_term) || isa<InvokeInst>(inst_before_term))
									insert_before = inst_before_term;
							}

							// Find the call that does not return and wrap up before it.
							foreach(Instruction& i, *bb) {
								CallInst* ci;
								if((ci = dyn_cast<CallInst>(&i)) && ci->doesNotReturn()) {
									insert_before = &i;
                                    non_returning_call = ci;

									// see if this is a call to exit, in which case we are going to have to deinit the profiler
									if(ci && ci->getCalledFunction() && ci->getCalledFunction()->getName() == "exit")
										is_exit_point = true;
									break;
								}
							}
						}

						if(add_logRegionExit_func) {
							op_args.push_back(ConstantInt::get(types.i64(),func_region_id));
							op_args.push_back(ConstantInt::get(types.i32(),Region::REGION_TYPE_FUNC));
							ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args);
							CallInst::Create(logRegionExit_func, *aref, "", insert_before);
							delete aref;
							op_args.clear();
						}

						// if this is "main" we insert call to _KDeinit
						if(!(non_returning_call && non_returning_call->getCalledFunction() &&  isCppThrowFunc(non_returning_call->getCalledFunction())) && 
                            (func.getName().compare("main") == 0 || func.getName().compare("MAIN__") == 0 || is_exit_point)) 
                        {
							ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args);
							if(add_deinitProfiler_func) {
								CallInst::Create(deinitProfiler_func, *aref, "", insert_before);
							}
							delete aref;
						}
					}

					bb_id++;
				}

				// insert calls to _KEnterRegion(region_id) and _KExitRegion(region_id) to all loops
				for(unsigned i = 0; i < function_loops.size(); ++i) {

					RegionId loop_id = loop_header_name_to_region_id[function_loops[i]->getHeader()->getName()];
					RegionId parent_id;

					// if this has a parent, we print edge in nesting_graph from parent to here
					if(Loop* parent = function_loops[i]->getParentLoop()) {
						parent_id = loop_header_name_to_region_id[parent->getHeader()->getName()];
					}
					else { // if no parent, then there is an edge from the function to this loop
						parent_id = func_region_id;
					}

					nesting_graph << "\t" << parent_id << " -> " << loop_id << ";\n";
					region_graph << parent_id << " " << loop_id << "\n";

					if(add_logRegionEntry_func) {
						instrumentLoop(function_loops[i],LI,loop_header_name_to_region_id[function_loops[i]->getHeader()->getName()],loop_body_region_ids[i],logRegionEntry_func,logRegionExit_func, logLandingPad_func, loop_to_landing_pads_map);
					}
				}

				if(add_logRegionEntry_func) {
					// finally, we insert call to _KEnterRegion() for the beginning of this function
					op_args.push_back(ConstantInt::get(types.i64(),func_region_id));
					op_args.push_back(ConstantInt::get(types.i32(),Region::REGION_TYPE_FUNC)); // 0 for 2nd arg means this is a function region
					ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args);
					CallInst::Create(logRegionEntry_func, *aref, "", func.getEntryBlock().getFirstNonPHI());
					delete aref;
					op_args.clear();
				}

				// if this happens to be main, we also need to call _KInit()
				if(add_initProfiler_func && (func.getName().compare("main") == 0 || func.getName().compare("MAIN__") == 0)) {
					ArrayRef<Value*> *aref = new ArrayRef<Value*>(op_args);
					CallInst::Create(initProfiler_func, *aref, "", func.getEntryBlock().getFirstNonPHI());
					delete aref;
				}
			} // end for loop
            foreach(Region& r, regions) {
                std::string encoded_region_str;
                new GlobalVariable(m, types.i8(), false, GlobalValue::ExternalLinkage, ConstantInt::get(types.i8(), 0), Twine(r.formatToString(encoded_region_str)));
            }
		} // end instrumentModule(...)

		void getAnalysisUsage(AnalysisUsage &AU) const {
			// XXX FIXME: this doesn't preserve the CFG!
			AU.setPreservesCFG();
			AU.addRequired<LoopInfo>();
		}
	};  // end of struct RegionInstrument

	char RegionInstrument::ID = 0;
	const std::string RegionInstrument::CPP_ENTRY_FUNC = "_GLOBAL__I_main";
	const std::string RegionInstrument::CPP_EXIT_FUNC = "__tcf_";
    const std::string RegionInstrument::CPP_THROW_FUNC_NAME = "__cxa_throw";
    const std::string RegionInstrument::CPP_RETHROW_FUNC_NAME = "_Unwind_Resume_or_Rethrow";
	const std::string RegionInstrument::REGIONS_INFO_FILENAME = "sregions.txt";

	RegisterPass<RegionInstrument> X("regioninstrument", "Region Instrumenter",
	  false /* Only looks at CFG */,
	  false /* Analysis Pass */);
} // end anon namespace

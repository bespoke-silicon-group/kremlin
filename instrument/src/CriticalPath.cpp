// XXX FIXME make sure callinst creations doesn't insert them into blocks yet
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
#include <functional>

#include <iostream>
#include <fstream>
#include <sstream>

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"

#include "foreach.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include "PassLog.h"
#include "OpCosts.h"
#include "UnaryFunctionConst.h"
#include "InstrumentationFuncManager.h"
#include "LLVMTypes.h"
#include "InstrumentationCall.h"
#include "InstrumentedCall.h"

#include <time.h>

using namespace llvm;

namespace {

	static cl::opt<std::string> moduleExtension("module-extension",cl::desc("Extension of file (defaults to .bc)"),cl::value_desc("extension"),cl::init(".bc"));
	static cl::opt<std::string> opCostFile("op-costs",cl::desc("File containing mapping between ops and their costs."),cl::value_desc("filename"),cl::init("__none__"));

	struct CriticalPath : public ModulePass {
		static char ID;
        static const std::string CPP_THROW_FUNC;
        static const std::string CPP_RETHROW_FUNC;
        static const std::string CPP_EH_EXCEPTION;
        static const std::string CPP_EH_TYPE_ID;
        static const std::string CPP_EH_SELECTOR;


		PassLog& log;

		OpCosts costs;
		std::map<std::string,unsigned int> uninstrumented_func_cost_map;

		std::map<BasicBlock*,BasicBlock*> idom;

		unsigned int basicBlockCount;

		std::map<BasicBlock*, unsigned int> basicBlockIdMap;
		std::map<std::string,Module*> definitionModule;
        boost::ptr_vector<InstrumentationCall> instrumentationCalls;

		CriticalPath() : ModulePass(ID), log(PassLog::get()) {}
		~CriticalPath() {}

		struct GetTerminator : public UnaryFunctionConst<BasicBlock*, Instruction*> {
			virtual Instruction* operator()(BasicBlock* bb) const {
				Instruction* terminator = bb->getTerminator();
				if(isa<UnreachableInst>(terminator)) {

					// Try call before terminator as the end.
					BasicBlock::InstListType& insts = bb->getInstList();
					if(insts.size() > 1) {

						BasicBlock::InstListType::reverse_iterator inst_before_term_it = insts.rbegin();
						inst_before_term_it++;
						Instruction* inst_before_term = &*inst_before_term_it;

						if(isa<CallInst>(inst_before_term) || isa<InvokeInst>(inst_before_term))
							terminator = inst_before_term;
					}

					// Try the function that doesn't return.
					foreach(Instruction& i, *bb) {
						if(CallInst* ci = dyn_cast<CallInst>(&i)) {
							if(ci->doesNotReturn()) {
								terminator = ci;
								break;
							}
						}
					}
				}
				return terminator;
			}
			virtual ~GetTerminator() {}
		};

		struct GetFirstNonPHI : public UnaryFunctionConst<BasicBlock*, Instruction*> {
			virtual Instruction* operator()(BasicBlock* bb) const {
				return bb->getFirstNonPHI();
			}
			virtual ~GetFirstNonPHI() {}
		};

		// Instrumentation calls are added to this class. It stores them and adds them to a particular location all at once at the end.
        // TODO: Depreciate
		class InstrumentationCalls {
			public:
			typedef std::map<CallInst*, Instruction*> CallInstToInst;
			typedef std::map<Instruction*, std::vector<CallInst*> > InstToCallInsts;
			typedef std::map<BasicBlock*, CallInst*> BBToCallInst;
			typedef UnaryFunctionConst<BasicBlock*, Instruction*> PutBefore;

			private:
			InstrumentationFuncManager& inst_funcs;     // Instrumentation functions

			InstToCallInsts inst_calls_for_after_inst;  // These CallInsts should be added after the CallInst of the Instruction
			InstToCallInsts inst_calls_for_before_inst; // These CallInsts should be added before the CallInst of the Instruction
			InstToCallInsts inst_calls_for_inst;        // These CallInsts are generated for the Instruction

			const PutBefore& put_before;                // Function that takes a basic block and returns the instruction to put the CallInsts before.

			CallInst* createCallInst(const std::string& name, const std::vector<Value*>& args) {
				return CallInst::Create(inst_funcs.get(name), args.begin(), args.end(), "");
			}

			void addCallInstToMap(Instruction* inst, CallInst* call_inst) {
				inst_calls_for_inst[inst].push_back(call_inst);
			}
			void addCallInstAfterToMap(Instruction* inst, CallInst* call_inst) {
				inst_calls_for_after_inst[inst].push_back(call_inst);
			}
			void addCallInstBeforeToMap(Instruction* inst, CallInst* call_inst) {
				inst_calls_for_before_inst[inst].push_back(call_inst);
			}

			public:
			InstrumentationCalls(InstrumentationFuncManager& inst_funcs, const PutBefore& put_before) : inst_funcs(inst_funcs), put_before(put_before) {}

			/**
			 * Adds an instrumentation call.
			 *
			 * Each additional call is appended to the list.
			 *
			 * @param inst The instruction that we are instrumenting.
			 * @param name The name of the instrumenation call.
			 * @param ,inst_to_idargs The arguments to the function.
			 */
			void addCallInst(Instruction* inst, const std::string& name, const std::vector<Value*>& args) {
				CallInst* call_inst = createCallInst(name, args);
				addCallInstToMap(inst, call_inst);
			}

			/**
			 * Adds an instrumentation call that should go after others. 
			 *
			 * Each additional call is appended to the list.
			 */
			void addCallInstAfter(Instruction* inst, const std::string& name, const std::vector<Value*>& args) {
				CallInst* call_inst = createCallInst(name, args);
				addCallInstAfterToMap(inst, call_inst);
			}

			/**
			 * Adds an instrumentation call that should go before others.
			 *
			 * Each additional call is prepended to the list.
			 */
			void addCallInstBefore(Instruction* inst, const std::string& name, const std::vector<Value*>& args) {
				CallInst* call_inst = createCallInst(name, args);
				addCallInstBeforeToMap(inst, call_inst);
			}

			std::vector<CallInst*>& getInstCallsForInst(Instruction* inst, std::vector<CallInst*>& result) {
				std::vector<CallInst*>* call_insts;

				call_insts = &inst_calls_for_before_inst[inst];
				result.insert(result.end(), call_insts->rbegin(), call_insts->rend());  // Must be in reverse order!

				call_insts = &inst_calls_for_inst[inst];
				result.insert(result.end(), call_insts->begin(), call_insts->end());

				call_insts = &inst_calls_for_after_inst[inst];
				result.insert(result.end(), call_insts->begin(), call_insts->end());

				return result;
			}

			std::vector<CallInst*>& getInstCallsForBlock(BasicBlock* blk, std::vector<CallInst*>& result) {
				for(BasicBlock::iterator inst = blk->begin(), insts_end = blk->end(); inst != insts_end; inst++) {
					getInstCallsForInst(inst, result);
				}
				return result;
			}

			void appendInstCallsToFunc(Function* func) {
				for(Function::iterator blk = func->begin(), last_blk = func->end(); blk != last_blk; ++blk)
					appendInstCallsToBlock(blk);
			}

			void appendInstCallsToBlock(BasicBlock* blk) {
				std::vector<CallInst*> inst_calls;
				getInstCallsForBlock(blk, inst_calls);

				Instruction* target_inst = put_before(blk);

				for(unsigned i = 0; i < inst_calls.size(); ++i)
					inst_calls[i]->insertBefore(target_inst);
			}
		};

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

		};

		/**
		 * Function object that returns if a block is a successor of the
		 * target block.
		 */
		class IsSuccessor : public std::unary_function<BasicBlock*, bool> {
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

			public:

			/**
			 * Initializes a new comparator.
			 *
			 * @param target The block to check for successors of.
			 */
			IsSuccessor(BasicBlock* target) : target(target) {
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
		};

		unsigned int getBasicBlockId(BasicBlock* blk) {
			std::map<BasicBlock*, unsigned int>::iterator it;
			if((it = basicBlockIdMap.find(blk)) != basicBlockIdMap.end())
				return it->second;
			unsigned int insertedId = basicBlockCount++;
			LOG_DEBUG() << "found basic block " << blk->getName() << ". assigning id " << insertedId << "\n";
			basicBlockIdMap.insert(std::pair<BasicBlock*, unsigned int>(blk, insertedId));
			return insertedId;
		}

		BasicBlock* getImmediateDominator(BasicBlock* blk) {
			DominatorTree &dt = getAnalysis<DominatorTree>(*blk->getParent());
			
			return dt.getNode(blk)->getIDom()->getBlock();
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
		static std::set<BasicBlock*>& getClosestPredecessors(BasicBlock* target, BasicBlock* predecessor, std::set<BasicBlock*>& results, std::set<BasicBlock*>& searched) {
			// don't go any further if we already search this predecessor
			if(searched.find(predecessor) != searched.end())
				return results;

			// mark that we have search this one
			searched.insert(predecessor);

			TerminatorInst* terminator = predecessor->getTerminator();

			if(isa<BranchInst>(terminator) || isa<SwitchInst>(terminator)) {
				// check all successors to predecessor
				for(unsigned int i = 0; i < terminator->getNumSuccessors(); i++) {
					BasicBlock* successor = terminator->getSuccessor(i);

					// if target is a successor, then we put this in results
					if(successor == target) {
						results.insert(predecessor);
					}

					// recurse, with successor as the next pred to look at
					getClosestPredecessors(target, successor,  results, searched);

					// if successor is part of the results, then this pred should also be there
					if(results.find(successor) != results.end())
						results.insert(predecessor);
				}
			}
			return results;
		}

		BasicBlock* findNearestCommonDominator(const std::set<BasicBlock*>& blocks) {
			BasicBlock* commonDominator = *blocks.begin();
			DominatorTree &dt = getAnalysis<DominatorTree>(*commonDominator->getParent());

			for(std::set<BasicBlock*>::iterator it = blocks.begin(), end = blocks.end(); it != end; it++)
			{
				commonDominator = dt.findNearestCommonDominator(commonDominator, *it);
				assert(commonDominator);
			}

			return commonDominator;
		}

		/**
		 * Returns the blocks that control the incoming blocks of the phi
		 * instruction.
		 *
		 * @param phi     The phi node to get the incoming block's conditions.
		 * @param result  A place to put the results.
		 * @return        The argument passed as the result.
		 */
		std::set<BasicBlock*>& getControllingBlocks(PHINode* phi, std::set<BasicBlock*>& result)
		{
			result.clear();

			BasicBlock* bb_containing_phi = phi->getParent();
			//BasicBlock* immediate_dominator = getImmediateDominator(bb_containing_phi);
			DominatorTree &dt = getAnalysis<DominatorTree>(*bb_containing_phi->getParent());


			std::map<Value*, std::set<BasicBlock*> > equivelent_values;

			// Create map between incoming values and the block(s) they come from.
			unsigned int num_incoming = phi->getNumIncomingValues();
			for(unsigned int i = 0; i < num_incoming; i++) {
				Value* incoming_value = phi->getIncomingValue(i);
				equivelent_values[incoming_value].insert(phi->getIncomingBlock(i));
			}

			// For each incoming value, we find the nearest common dominator of all incoming
			// blocks that contain that value, adding it to the incoming_blocks set.
			std::set<BasicBlock*> incoming_blocks;
			for(std::map<Value*, std::set<BasicBlock*> >::iterator it = equivelent_values.begin(), end = equivelent_values.end(); it != end; it++)
				incoming_blocks.insert(findNearestCommonDominator(it->second));

			// Add all the controlling blocks of the incoming blocks.
			for(std::set<BasicBlock*>::iterator it = incoming_blocks.begin(), end = incoming_blocks.end(); it != end; it++)
			{
				getControllingBlocks(*it, idom[bb_containing_phi], dt, result);
			}
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
		std::set<BasicBlock*>& getControllingBlocks(BasicBlock* target, BasicBlock* limit, DominatorTree& dt, std::set<BasicBlock*>& result)
		{
			if(!dt.dominates(limit, target))
				return result;

			IsSuccessor is_successor(target);

			//Add the controlling block
			BasicBlock* controlling_block;

			//Only add the controlling block if we haven't seen it before and it's not null.
			if( (controlling_block = getControllingBlock(target, is_successor(target))) 
			  && result.find(controlling_block) == result.end()
			  ) {
				result.insert(controlling_block);

				//Add the controlling block of the found controlling block.
				return getControllingBlocks(controlling_block, limit, dt, result);
			}
			return result;
		}

		/**
		 * Returns the virtual register number of the condition of the block or 0
		 * if this block did not terminate with a condition.
		 *
		 * @param blk            The block to get the condition id of.
		 * @param inst_to_id     The map containing the instruction to id map.
		 * @return               The id of the condition or 0 if this block had no
		 *                       condition.
		 */
		unsigned int getConditionIdOfBlock(BasicBlock* blk, const std::map<Value*, unsigned int>& inst_to_id) const {
			TerminatorInst* terminator = blk->getTerminator();
			BranchInst* br_inst;

			unsigned ret_val = 0;

			// search for condition (if branch or switch inst)
			if(isa<BranchInst>(terminator) && (br_inst = cast<BranchInst>(terminator))->isConditional()) {
				Value* cond = br_inst->getCondition();

				if(!isa<Constant>(cond)) {
					ret_val = inst_to_id.find(br_inst->getCondition())->second;
				}
			}
			else if(isa<SwitchInst>(terminator)) { 
				SwitchInst* sw_inst = cast<SwitchInst>(terminator);
				Value* cond = sw_inst->getCondition();

				if(!isa<Constant>(cond)) {
					ret_val = inst_to_id.find(sw_inst->getCondition())->second;
				}
			} 
			return ret_val; // no conditional branch or switch so there is no ID
		}

		/**
		 * Returns the closest block in the CFG that has a control condition
		 * that could lead away from executing this block.
		 *
		 * This actually looks for the node in the post-domininance frontier
		 * that isn't itself.
		 *
		 * @param blk The block to find the control condition.
		 * @Return the closest controlling block.
		 */
		BasicBlock* getControllingBlock(BasicBlock* blk)
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
		BasicBlock* getControllingBlock(BasicBlock* blk, bool consider_self)
		{
			LOG_DEBUG() << "Controlling block search of " << blk->getName() << "\n";

			BasicBlock* ret_val = NULL;

			PostDominanceFrontier &PDF = getAnalysis<PostDominanceFrontier>(*blk->getParent());
			DominatorTree &DT = getAnalysis<DominatorTree>(*blk->getParent());

			//LOG_DEBUG() << blk->getName() << "\n";


			// get post dominance frontier for blk
			DominanceFrontierBase::iterator dsmt_it = PDF.find(blk);

			// sanity check to make sure an entry in the PDF exists for this blk
			if(dsmt_it == PDF.end())
			{
				LOG_ERROR() << "Could not find blk " << blk->getName() << 
					" in post dominance frontier of function " << blk->getParent()->getName() <<
					"! Contents of the pdf:"<< "\n";

				for(PostDominanceFrontier::iterator it = PDF.begin(), end = PDF.end(); it != end; it++)
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
			for(DominanceFrontierBase::DomSetType::iterator dst_it = dsmt_it->second.begin(), dst_end = dsmt_it->second.end(); dst_it != dst_end; ++dst_it) {
				BasicBlock* candidate = *dst_it;

				LOG_DEBUG() << "looking at " << candidate->getName() << "\n";

				if((consider_self || candidate != blk) && DT.dominates(candidate,blk)) { // not itself so we found the controlling blk
					//LOG_DEBUG() << "found controlling blk: " << candidate->getName() << "\n";
					ret_val = candidate;
					break;
				}
			}

			return ret_val;
		}

		// checks whether this returns a non-ptr value (not void)
		bool returnsRealValue(const Type* ret_type) {
			//if(called_func->getReturnType()->getTypeID() != Type::VoidTyID && called_func->getReturnType()->getTypeID() != Type::PointerTyID)
			if(ret_type->getTypeID() != Type::VoidTyID && ret_type->getTypeID() != Type::PointerTyID)
				return true;
			else
				return false;
		}

		bool returnsRealValue(CallInst* ci) {
			return returnsRealValue(ci->getType());
		}

		bool returnsRealValue(Function* func) {
			return returnsRealValue(func->getReturnType());
		}

		bool returnsRealValue(InvokeInst* ii) {
			return returnsRealValue(ii->getType());
		}

		// This function tries to untangle some strangely formed function calls.
		// If the call inst is a normal call inst then it just returns the function that is returned by
		// ci->getCalledFunction().
		// Otherwise, it checks to see if the first op of the call is a constant bitcast op that can
		// result from LLVM not knowing the function declaration ahead of time. If it detects this
		// situation, it will grab the function that is being cast and return that.
		template <typename Callable>
		Function* untangleCall(Callable* ci)
		{
			if(ci->getCalledFunction()) { return ci->getCalledFunction(); }

			Value* op0 = ci->getCalledValue(); // TODO: rename this to called_val
			if(!isa<User>(op0)) {
				LOG_DEBUG() << "skipping op0 of callinst because it isn't a User: " << *op0 << "\n";

				return NULL;
			}

			User* user = cast<User>(op0);

			Function* called_func = NULL;

			// check if the user is a constant bitcast expr with a function as the first operand
			if(isa<ConstantExpr>(user)) { 
				if(isa<Function>(user->getOperand(0))) {
					//LOG_DEBUG() << "untangled function ref from bitcast expr\n";
					called_func = cast<Function>(user->getOperand(0));
				}
			}

			return called_func;
		}

		// Returns true if these blocks and the blocks between them could have been joined together into a single basic block.
		bool isSingleInstructionStream(BasicBlock* block1, BasicBlock* block2) {
			assert(block1->getParent() == block2->getParent() && "These blocks must come from the same function!");

			DominatorTree &dt = getAnalysis<DominatorTree>(*block1->getParent());

			if(block1 == block2)
				return true;

			if(dt.properlyDominates(block2, block1))
				return isSingleInstructionStream(block2, block1);

			BranchInst* br;
			if(dt.properlyDominates(block1, block2) && (br = dyn_cast<BranchInst>(block1->getTerminator())) && br->isUnconditional())
				return isSingleInstructionStream(br->getSuccessor(0), block2);

			return false;
		}

		// Returns a map from basic blocks in a function to blocks that they could be joined together 
		// to join a larger basic block. The value pointed to is the block in this set of blocks that 
		// can be joined together that dominates the rest.
		std::map<BasicBlock*, BasicBlock*>& getJoinedBasicBlocks(Function* func, std::map<BasicBlock*, BasicBlock*>& result) {
			DominatorTree &dt = getAnalysis<DominatorTree>(*func);

			std::set<BasicBlock*> remaining;
			foreach(BasicBlock& blk, *func)
				remaining.insert(&blk);

			while(!remaining.empty()) {
				std::set<BasicBlock*>::iterator begin = remaining.begin(); // get a random element out of the remaining elements
				BasicBlock* target_blk = *begin;
				//remaining.erase(begin);

				BasicBlock* highest_dominator = target_blk; // the block that dominates the rest
				std::set<BasicBlock*> contiguous_blks;      // find blocks that can be joined together and join them
				contiguous_blks.insert(target_blk);

				foreach(BasicBlock* blk, remaining) {
					if(isSingleInstructionStream(target_blk, blk)) {
						contiguous_blks.insert(blk);
						if(dt.properlyDominates(blk, highest_dominator))
							highest_dominator = blk;
					}
				}

//				LOG_DEBUG() << "contiugous blocks of " << target_blk->getName() << "\n";
				foreach(BasicBlock* blk, contiguous_blks) {
					remaining.erase(blk);                   // these should no longer be considered
					result[blk] = highest_dominator;        // make them all point to the same block because they are now equivelent.
//					LOG_DEBUG() << blk->getName() << "\n";
				}
			}
			return result;
		}

		// Returns a set of landing pads for invoke instructions. Blocks that could have been joined together into larger basic blocks are
		// considered the same and the one that dominates the rest is returned.
		std::set<BasicBlock*>& getInvokeLandingPads(Function* func, std::set<BasicBlock*>& result) {

			std::map<BasicBlock*, BasicBlock*> joined_blocks;
			getJoinedBasicBlocks(func, joined_blocks);
/*
			LOG_DEBUG() << "joined blocks found for getInvokeLandingPads:" << "\n";
			for(std::map<BasicBlock*, BasicBlock*>::iterator it = joined_blocks.begin(), end = joined_blocks.end(); it != end; it++)
				LOG_DEBUG() << it->first->getName() << " -> " << it->second->getName() << "\n";
*/

			foreach(BasicBlock& blk, *func)
				foreach(Instruction& inst, blk)
					if(InvokeInst* ii = dyn_cast<InvokeInst>(&inst))
						result.insert(joined_blocks[ii->getUnwindDest()]);
			return result;
		}

		std::set<BasicBlock*>& getInvokeDestinations(Function* func, std::set<BasicBlock*>& result) {

			std::map<BasicBlock*, BasicBlock*> joined_blocks;
			getJoinedBasicBlocks(func, joined_blocks);

			foreach(BasicBlock& blk, *func)
				foreach(Instruction& inst, blk) {
					if(InvokeInst* ii = dyn_cast<InvokeInst>(&inst))
						result.insert(joined_blocks[ii->getNormalDest()]);
				}
			return result;
		}

		//returns a vector of basic blocks of joined basic blocks by branch always and that only have one predecessor
		std::vector<BasicBlock*>& joinBasicBlocks(BasicBlock* first_blk, std::vector<BasicBlock*>& result) {
			result.push_back(first_blk);

			TerminatorInst* term = first_blk->getTerminator();
			BranchInst* br;
			BasicBlock* successor;
			if((br = dyn_cast<BranchInst>(term)) && isSingleInstructionStream(first_blk, successor = br->getSuccessor(0))) {
				joinBasicBlocks(successor, result);
			}
			return result;
		}

		/**
         * returns a vector of instructions of joined basic blocks by branch always and that only have one predecessor.
         *
         * @param first_blk The block to start the search.
         * @param result The vector of instructions making up the joined blocks.
         */
		std::vector<Instruction*>& joinBasicBlocks(BasicBlock* first_blk, std::vector<Instruction*>& result) {
			std::vector<BasicBlock*> blocks;

			joinBasicBlocks(first_blk, blocks);

			foreach(BasicBlock* blk, blocks)
				foreach(Instruction& inst, *blk)
					if(!isa<TerminatorInst>(&inst))
						result.push_back(&inst);

			result.push_back(blocks.back()->getTerminator());
			return result;
		}

		template <typename Callable>
		void instrumentCall(Callable* ci, std::map<Value*,unsigned int>& inst_to_id, InstrumentationCalls& front, uint64_t bb_call_idx) {
			LLVMTypes types(ci->getContext());
			std::vector<Value*> args;

			// don't do anything for LLVM instrinsic functions since we know we'll never instrument those functions
			if(!(ci->getCalledFunction() && ci->getCalledFunction()->isIntrinsic())) {
				Function* called_func = untangleCall(ci);

				if(called_func) // not a func ptr
					LOG_DEBUG() << "got a call to function " << called_func->getName() << "\n";
				else
					LOG_DEBUG() << "got a call to function via function pointer: " << *ci;

				// insert call to prepareCall to setup the structures needed to pass arg and return info efficiently
                InstrumentedCall<Callable>* instrumented_call;
                instrumentationCalls.push_back(instrumented_call = new InstrumentedCall<Callable>(ci, bb_call_idx));

                args.push_back(ConstantInt::get(types.i64(),instrumented_call->getId()));   // Call site ID
                args.push_back(ConstantInt::get(types.i64(),0));                            // ID of region being called. TODO: Implement
				front.addCallInst(ci, "prepareCall", args);
                args.clear();

				// if this call returns a value, we need to call addReturnValueLink
				if(returnsRealValue(ci)) {
					args.push_back(ConstantInt::get(types.i32(),inst_to_id[ci])); // dest ID
					front.addCallInst(ci, "addReturnValueLink", args);
					args.clear();
				}

				// link all the args that aren't passed by ref
				CallSite cs = CallSite::get(ci);

				for(CallSite::arg_iterator arg_it = cs.arg_begin(), arg_end = cs.arg_end(); arg_it != arg_end; ++arg_it) {
					Value* the_arg = *arg_it;
					LOG_DEBUG() << "checking arg: " << *the_arg << "\n";

					if(!isa<PointerType>(the_arg->getType())) {
						if(!isa<Constant>(*arg_it)) { // not a constant so we need to call linkArgToAddr
							args.push_back(ConstantInt::get(types.i32(),inst_to_id[the_arg])); // src ID

							front.addCallInst(ci, "linkArgToLocal", args);

							args.clear();
						}
						else { // this is a constant so call linkArgToConst instead (which takes no args)
							front.addCallInst(ci, "linkArgToConst", args);
						}
					}
				} // end for(arg_it)
			}
			else {
				LOG_DEBUG() << "ignoring call to LLVM intrinsic function: " << ci->getCalledFunction()->getName() << "\n";
			}
		}

		// Gets the induction var increment. This used to be part of LLVM but was removed for reasons
		// unbeknownst to me :(
		Instruction *getCanonicalInductionVariableIncrement(PHINode* ind_var, Loop* loop) const {
			bool P1InLoop = loop->contains(ind_var->getIncomingBlock(1));
			return cast<Instruction>(ind_var->getIncomingValue(P1InLoop));
		}
		
		// adds all the canon ind. var increments for a loop (including its subloops) to canon_incs set
		void addCIVIToSet(Loop* loop, std::set<PHINode*>& canon_indvs, std::set<Instruction*>& canon_incs) {

			std::vector<Loop*> sub_loops = loop->getSubLoops();

			// check all subloops for canon var increments
			if(sub_loops.size() > 0) {
				for(std::vector<Loop*>::iterator sl_it = sub_loops.begin(), sl_end = sub_loops.end(); sl_it != sl_end; ++sl_it) 
					addCIVIToSet(*sl_it,canon_indvs,canon_incs);
			}

			PHINode* civ = loop->getCanonicalInductionVariable();

			// couldn't find an ind var so there is nothing left to do for this loop
			if(civ == NULL) { return; }

			Instruction* civ_inc = getCanonicalInductionVariableIncrement(civ,loop);

			//LOG_DEBUG() << "found canon ind var increment: " << civi->getName() << "\n";
			canon_indvs.insert(civ);
			canon_incs.insert(civ_inc);
		}

		// returns vector of instructions that are in loop (but not a subloop of loop) and use the Value val
		std::vector<Instruction*> getUsesInLoop(LoopInfo& LI, Loop* loop, Value* val) {
			std::vector<Instruction*> uses;

			for(Value::use_iterator ui = val->use_begin(), ue = val->use_end(); ui != ue; ++ui) {
				//LOG_DEBUG() << "\tchecking use: " << **ui;

				if(isa<Instruction>(*ui)) {
					Instruction* i = cast<Instruction>(*ui);
					//Loop* user_loop = LI.getLoopFor(i->getParent());

					//LOG_DEBUG() << "\t\tparent loop for use has header " << user_loop->getHeader()->getName() << "\n";

					if(!isa<PHINode>(i) && loop == LI.getLoopFor(i->getParent())) {
						uses.push_back(i);
					}
				}
			}
			
			return uses;
		}

		bool isReductionOpType(Instruction* inst) {
			if( inst->getOpcode() == Instruction::Add
			  || inst->getOpcode() == Instruction::FAdd
			  || inst->getOpcode() == Instruction::Sub
			  || inst->getOpcode() == Instruction::FSub
			  || inst->getOpcode() == Instruction::Mul
			  || inst->getOpcode() == Instruction::FMul
			  ) { return true; }
			else { return false; }
		}

		Instruction* getReductionVarOp(LoopInfo& LI, Loop* loop, Value *val) {
			std::vector<Instruction*> uses_in_loop = getUsesInLoop(LI,loop,val);

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

		Instruction* getArrayReductionVarOp(LoopInfo& LI, Loop* loop, Value *val) {
			std::vector<Instruction*> uses_in_loop = getUsesInLoop(LI,loop,val);

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

		void getArrayReductionVars(LoopInfo& LI, Loop* loop, std::set<Instruction*>& red_var_ops) {
			// The pattern we'll look for is a pointer used in 2 getelementptr insts that are in the same block,
			// with the first being used by a load, the second by a store, and the load being used by an associative
			// op

			// loop through basic blocks in loop
			std::vector<BasicBlock*> loop_blocks = loop->getBlocks();

			//for(Loop::block_iterator* bb = loop->block_begin(), bb_end = loop->block_end(); bb != bb_end; ++bb) {
			for(unsigned j = 0; j < loop_blocks.size(); ++j) {
				BasicBlock* bb = loop_blocks[j];
				std::vector<GetElementPtrInst*> geps;

				// find all the GEP instructions
				for(BasicBlock::iterator inst = bb->begin(), inst_end = bb->end(); inst != inst_end; ++inst) {
					if(isa<GetElementPtrInst>(inst)) {
						geps.push_back(cast<GetElementPtrInst>(inst));
					}
				}

				// now we'll see how many pairs of GEP insts in this block use the same pointer
				std::map<Value*,unsigned> gep_ptr_uses_in_bb;

				for(unsigned i = 0; i < geps.size(); ++i) {
					//Value* gep_ptr = geps[i]->getPointerOperand();
					//LOG_INFO() << "gep ptr op: " << *gep_ptr << "\n";

					gep_ptr_uses_in_bb[geps[i]->getPointerOperand()]++;
				}

				for(std::map<Value*,unsigned>::iterator gp_it = gep_ptr_uses_in_bb.begin(), gp_end = gep_ptr_uses_in_bb.end(); gp_it != gp_end; ++gp_it) {
					if((*gp_it).second == 2) {
						// First condition met.
						// We'll now check the rest of the conditions now to make sure they work.

						if(Instruction* red_var_op = getArrayReductionVarOp(LI,loop,(*gp_it).first)) {
							LOG_WARN() << "identified reduction variable increment (array, used in function: " << red_var_op->getParent()->getParent()->getName() << "): " << *red_var_op << "\n";
							LOG_WARN() << "\treduction var: " << *(*gp_it).first << "\n";
							red_var_ops.insert(red_var_op);
						}
					}
				}
			}
		}

		void getReductionVars(LoopInfo& LI, Loop* loop, std::set<Instruction*>& red_var_ops) {
			BasicBlock* header = loop->getHeader();
			LOG_DEBUG() << "checking for red vars in loop headed by " << header->getName() << " (func = " << header->getParent()->getName() << ")\n";

			std::vector<Loop*> sub_loops = loop->getSubLoops();

			// check all subloops for reduction vars
			if(sub_loops.size() > 0) {
				for(std::vector<Loop*>::iterator sl_it = sub_loops.begin(), sl_end = sub_loops.end(); sl_it != sl_end; ++sl_it) 
					getReductionVars(LI,*sl_it,red_var_ops);
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

					if(Instruction* red_var_op = getReductionVarOp(LI,loop,gi)) {
						LOG_DEBUG() << "identified reduction variable increment (global, used in function: " << red_var_op->getParent()->getParent()->getName() << "): " << *red_var_op << "\n";
						LOG_DEBUG() << "\treduction var: " << *gi << "\n";
						red_var_ops.insert(red_var_op);
					}
				}

				// check for array reduction.
				getArrayReductionVars(LI,loop,red_var_ops);
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
				std::vector<Instruction*> uses_in_loop = getUsesInLoop(LI,loop,phi_it);

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

		bool isThrowCall(CallInst* ci) {
			Function* func = ci->getCalledFunction();
			if(func) {
				return func->getName() == CPP_THROW_FUNC && ci->doesNotReturn();
			}
			else {
				return false;
			}
		}

		virtual bool runOnModule(Module &m) {

			// If we specified a file for op costs, let's read those in.
			// Note that we don't require that the file contain all (or even any) of the ops. Those note
			// defined will retain the default op cost.
			if(opCostFile != "__none__") {
				costs.parseFromFile(opCostFile);
			}

			LOG_DEBUG() << costs;

			uninstrumented_func_cost_map["sin"] = 10;
			uninstrumented_func_cost_map["cos"] = 10;
			uninstrumented_func_cost_map["log"] = 10;
			uninstrumented_func_cost_map["sqrt"] = 10;

			uninstrumented_func_cost_map["ceil"] = 1;
			uninstrumented_func_cost_map["floor"] = 1;
			uninstrumented_func_cost_map["fabs"] = 1;

			uninstrumented_func_cost_map["feof"] = 2;

			unsigned int op_id = 1; // use this for a unique function

			basicBlockCount = 1; // start at 1 to reserve 0 for "NULL"
			basicBlockIdMap.clear();

			// Instrument the module
			instrumentModule(m,op_id);

			log.close();

			return true;
		}

		// For all non-pointer args, associates ID with arg and also inserts call to transferAndUnlinkArg()
		void setupFuncArgs(Function* func, std::map<Value*,unsigned int>& inst_to_id, unsigned int& curr_id, InstrumentationCalls& front, bool init_to_const) {
			LLVMTypes types(func->getContext());
			std::vector<Value*> op_args;

			for(Function::arg_iterator arg_it = func->arg_begin(), arg_end = func->arg_end(); arg_it != arg_end; ++arg_it) {
				//cerr << "checking arg " << arg_it->getName() << "\n";

				if(!isa<PointerType>(arg_it->getType())) {
					//cerr << arg_it->getName() << " is passed by value...\n";

					inst_to_id[arg_it] = curr_id;

					op_args.push_back(ConstantInt::get(types.i32(),curr_id)); // dest ID

					// if we want to initialize to constants (e.g. in main), then we insert call to linkAssignmentConst, otherwise we use transferAndUnlinkArg to set init values
					std::string func_to_insert = init_to_const ? "logAssignmentConst":"transferAndUnlinkArg";

					// insert at the very beginning of the function
					front.addCallInst(func->begin()->begin(), func_to_insert, op_args);
					op_args.clear();

					curr_id++;
				}
			}
		}

		// Maps every instruction (that we care about) to an ID. This will prevent us from racing on which BB is porcessed
		// first.
		void mapInstIDs(Function* func, std::map<Value*,unsigned int>& inst_to_id, unsigned int& curr_id) {
			for (Function::iterator blk = func->begin(), blk_end = func->end(); blk != blk_end; ++blk) {
				for (BasicBlock::iterator i = blk->begin(), inst_end = blk->end(); i != inst_end; ++i) {
						// if we had to assign this an ID earlier (probably a PHI) then don't give it another one now
						if(inst_to_id[i] != 0) { continue; }

						if(isa<BinaryOperator>(i) 
						  || isa<CastInst>(i) && isa<Constant>(cast<CastInst>(i)->getOperand(0))
						  || isa<CmpInst>(i) 
						  || isa<LoadInst>(i)
						  || isa<PHINode>(i)
						  || isa<SelectInst>(i) 
						  || isa<InsertValueInst>(i)
						  || isa<ExtractValueInst>(i)
						  // TODO: callinst's returning pointers shouldn't get values?
						  // TODO: we probably don't need ID for func that returns void
						  || isa<CallInst>(i)
						  || isa<InvokeInst>(i)
						  || isa<AllocaInst>(i)
						  ) {
							inst_to_id[i] = curr_id;
							curr_id++;
						}
						// if a cast of non-const, then we just use the ID from the cast's op
						else if(isa<CastInst>(i)) {
							Value* op = cast<CastInst>(i)->getOperand(0);
							unsigned int op_id = inst_to_id[op];

							// If op hasn't been assigned an ID yet, we'll give it one now.
							// Otherwise, we just use the op's ID for this inst
							if(op_id == 0) {
								inst_to_id[i] = curr_id;
								inst_to_id[op] = curr_id;

								curr_id++;
							}
							else {
								inst_to_id[i] = op_id;
							}
						}
				}
			}
		}

		// Inserts isntrumentation function calls at the end of block for all basic blocks in the specified function.
		void insertInstrumentationCalls(Function* func, std::map<BasicBlock*,std::vector<CallInst*> > inst_calls) {
			for(Function::iterator blk = func->begin(), last_blk = func->end(); blk != last_blk; ++blk) {
				// TODO: if this terminator is an unreachable inst, we won't execute these inst calls!
				Instruction* terminator = blk->getTerminator();

				for(unsigned i = 0; i < inst_calls[blk].size(); ++i) {
					inst_calls[blk][i]->insertBefore(terminator);
				}
			}
		}

        bool isCppThrowFunc(Instruction* i)
        {
            CallInst* ci;
            Function* func;
            return (ci = dyn_cast<CallInst>(i)) && (func = ci->getCalledFunction()) && (
                func->getName() == CPP_THROW_FUNC ||
                func->getName() == CPP_RETHROW_FUNC ||
                func->getName() == CPP_EH_EXCEPTION ||
                func->getName() == CPP_EH_TYPE_ID); // ||
                // func->getName() == CPP_EH_SELECTOR);
        }
        
		bool willInstrument(Instruction* i) {
			// TODO: fill in the rest?
			return isa<BinaryOperator>(i) || 
				isa<CmpInst>(i) || 
				isa<SelectInst>(i) || 
				isa<PHINode>(i) || 
				isa<CallInst>(i) && !isCppThrowFunc(i);
		}
			
		// returns true if val is a pointer to n-bit int
		bool isNBitIntPointer(Value* val, unsigned n) {
			const Type* val_type = val->getType();

			LOG_INFO() << *val;

			assert(isa<PointerType>(val_type) && "value is not even a pointer");

			const Type* val_ele_type = cast<PointerType>(val_type)->getElementType();

			return isa<IntegerType>(val_ele_type) && (cast<IntegerType>(val_ele_type)->getBitWidth() == n);
		}

		timespec diff(timespec start, timespec end) {
			timespec temp;
			if ((end.tv_nsec-start.tv_nsec)<0) {
				temp.tv_sec = end.tv_sec-start.tv_sec-1;
				temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
			} else {
				temp.tv_sec = end.tv_sec-start.tv_sec;
				temp.tv_nsec = end.tv_nsec-start.tv_nsec;
			}
			return temp;
		}


		void instrumentNonPHIInsts(BasicBlock* blk, std::set<Instruction*> canon_incs, std::set<Instruction*> red_var_ops, std::map<Value*,unsigned int>& inst_to_id, InstrumentationCalls& inst_calls_begin, InstrumentationCalls& inst_calls_end) {
			LLVMTypes types(blk->getContext());

			std::vector<Value*> args; // holds arguments passed to instrumentation functions

			InvokeInst* ii;
			uint64_t bb_call_idx = 0;
			for (BasicBlock::iterator i = blk->getFirstNonPHI(), inst_end = blk->end(); i != inst_end; ++i) {

				LOG_DEBUG() << "processing inst: " << PRINT_VALUE(*i) << "\n";

				if(isa<BinaryOperator>(i) || isa<CmpInst>(i) || isa<SelectInst>(i)) {

						// assign an ID to the current instruction
						switch(i->getOpcode()) {
							case Instruction::Add:
								args.push_back(ConstantInt::get(types.i32(),costs.int_add));
								break;
							case Instruction::FAdd:
								args.push_back(ConstantInt::get(types.i32(),costs.fp_add));
								break;
							case Instruction::Sub:
								args.push_back(ConstantInt::get(types.i32(),costs.int_sub));
								break;
							case Instruction::FSub:
								args.push_back(ConstantInt::get(types.i32(),costs.fp_sub));
								break;
							case Instruction::Mul:
								args.push_back(ConstantInt::get(types.i32(),costs.int_mul));
								break;
							case Instruction::FMul:
								args.push_back(ConstantInt::get(types.i32(),costs.fp_mul));
								break;
							case Instruction::UDiv:
							case Instruction::SDiv:
							case Instruction::URem:
							case Instruction::SRem:
								args.push_back(ConstantInt::get(types.i32(),costs.int_div));
								break;
							case Instruction::FDiv:
							case Instruction::FRem:
								args.push_back(ConstantInt::get(types.i32(),costs.fp_div));
								break;
							case Instruction::Shl:
							case Instruction::LShr:
							case Instruction::AShr:
							case Instruction::And:
							case Instruction::Or:
							case Instruction::Xor:
								args.push_back(ConstantInt::get(types.i32(),costs.logic));
								break;
							case Instruction::ICmp:
								args.push_back(ConstantInt::get(types.i32(),costs.int_cmp));
								break;
							case Instruction::FCmp:
								args.push_back(ConstantInt::get(types.i32(),costs.fp_cmp));
								break;
							case Instruction::Select:
								args.push_back(ConstantInt::get(types.i32(),0));
								break;
							default:
								//args.push_back(ConstantInt::get(types.i32(),0));
								//LOG_DEBUG() << "unknown opcode (" << i->getOpcode() << ") for binop/icmp: " << *i;
								log.close();
								assert(0 && "unknown opcode for binop/icmp");
								break;
						}

						Value* op0;
						Value* op1;

						// if this is a select inst, we need to call addControlDep before using the condition of the op
						if(isa<SelectInst>(i)) {
							SelectInst* sel_inst = cast<SelectInst>(i);
							op0 = sel_inst->getTrueValue();
							op1 = sel_inst->getFalseValue();

							std::vector<Value*> args2;

							args2.push_back(ConstantInt::get(types.i32(),inst_to_id[sel_inst->getCondition()]));

							inst_calls_end.addCallInst(i,"addControlDep",args2);

							// call to removeInit after select inst
							args2.clear();
						}
						else {
							op0 = i->getOperand(0);
							op1 = i->getOperand(1);
						}

						if(red_var_ops.find(i) != red_var_ops.end()) { // special function if this is a reduction variable op
							args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

							inst_calls_end.addCallInst(i,"logReductionVar",args);
						}
						else if(isa<Constant>(op0) || isa<Constant>(op1)) { // const propagation should stop these from both being constant

							std::string func_to_use; // will contain name of instrumentation function to call

							// if this inst is an induction var, we use a special logging function
							if(canon_incs.find(i) != canon_incs.end()) {
								args.clear();

								func_to_use = "logInductionVar";
							}
							// if both ops are constant, then we should use logAssignmentConst
							else if(isa<Constant>(op0) && isa<Constant>(op1)) {
								args.clear();

								LOG_DEBUG() << "NOTE: both operands are constant... did you forget to run constant propagation before this?\n";

								func_to_use = "logAssignmentConst";
							}
							else if(isa<Constant>(op0)) {
								args.push_back(ConstantInt::get(types.i32(),inst_to_id[op1])); // src ID
								func_to_use = "logBinaryOpConst";
							}
							else { // op1 is a constant
								args.push_back(ConstantInt::get(types.i32(),inst_to_id[op0])); // src ID
								func_to_use = "logBinaryOpConst";
							}

							args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

							inst_calls_end.addCallInst(i,func_to_use,args);
						}
						else { // going to use logBinaryOp() function
							LOG_DEBUG() << "inserting call to logBinaryOp\n";

							args.push_back(ConstantInt::get(types.i32(),inst_to_id[op0])); // src0 ID
							args.push_back(ConstantInt::get(types.i32(),inst_to_id[op1])); // src1 ID
							args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

							inst_calls_end.addCallInst(i,"logBinaryOp",args);
						}

						// if this was a select, we need to end the control dep now that we have the correct logging func inserted
						if(isa<SelectInst>(i)) {
							args.clear();
							inst_calls_end.addCallInst(i,"removeControlDep",args);
						}

						args.clear();
				} // end binop, icmp, selectinst

				else if(isa<CastInst>(i) && isa<Constant>(cast<CastInst>(i)->getOperand(0))) {
					Value* op = i->getOperand(0);

					// if this is a cast of a constant, we create a logAssignmentConst for this cast instruction
					if(isa<Constant>(op)) {
						LOG_DEBUG() << "inst is cast of a constant\n";

						args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

						inst_calls_end.addCallInst(i,"logAssignmentConst",args);

						args.clear();
					}
				} // end cast

				// TODO: combine this with cast inst?
				else if(isa<AllocaInst>(i)) {
					LOG_DEBUG() << "inst is an alloca\n";
					args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

					inst_calls_end.addCallInst(i,"logAssignmentConst",args);

					args.clear();
				}

				// insertvalue takes a struct and inserts a value into a specified position in the struct, returning the updated struct
				// We use logInsertValue/logInsertValueConst to track the dependencies between the inserted value and the struct into
				// which it is inserted. Note we can't just use logAssignment here because if we happen to insert a constant after
				// inserting another value, the struct's timestamp will use the constant time instead of the more accurate other value
				// time.
				// TODO: track the individual parts of the struct that are changing rather than the whole struct
				else if(isa<InsertValueInst>(i)) {
					LOG_DEBUG() << "inst is an insertvalue\n";

					if(!isa<Constant>(i->getOperand(1))) {
						// get ID of value that we will be updating the struct with
						args.push_back(ConstantInt::get(types.i32(),inst_to_id[i->getOperand(1)])); // src ID
					}

					// If the input struct is undefined, we want the dest to be to the current insertvalue inst.
					// Otherwise, we use the dest that is given in the instruction
					if(isa<Constant>(i->getOperand(0))) {
						args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID (current inst)
					}
					else {
						args.push_back(ConstantInt::get(types.i32(),inst_to_id[i->getOperand(0)])); // dest ID
					}

					if(isa<Constant>(i->getOperand(1))) {
						inst_calls_end.addCallInst(i,"logInsertValueConst",args);
					}
					else {
						inst_calls_end.addCallInst(i,"logInsertValue",args);
					}

					args.clear();
				}

				// extractvalue just returns the specified index of a specified struct
				// currently we just use logAssignment; in the future we should have a new function which takes into account the index being extracted
				else if(isa<ExtractValueInst>(i)) {
					LOG_DEBUG() << "inst is an extractvalue\n";

					args.push_back(ConstantInt::get(types.i32(),inst_to_id[i->getOperand(0)])); // src ID
					args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

					// log assignment between the struct and the resulting value we pull out of it
					inst_calls_end.addCallInst(i,"logAssignment",args);

					args.clear();
				}

				// need to add in calls to setup the necessary transfer of function arguments
				// Cannot insert prep call before throwing an exception because it is allocated beyond the stack pointer.
				// Making any calls after the value has been allocated will corrupt the value.
				else if(isa<CallInst>(i) && !isThrowCall(cast<CallInst>(i))) {
					CallInst* ci = cast<CallInst>(i);
					instrumentCall(ci, inst_to_id, inst_calls_begin, bb_call_idx++);

					Function* called_func = untangleCall(ci);

					// calls to malloc and calloc get logMalloc(addr, size) call
					if(called_func 
					  && (called_func->getName() == "malloc" || called_func->getName() == "calloc")
					  ) {
						LOG_DEBUG() << "inst is a call to malloc/calloc\n";

						// insert address (return value of callinst)
						LOG_DEBUG() << "adding return value of inst as arg to logMalloc\n";
						args.push_back(ci);

						// insert size (arg 0 of func)
						Value* sizeOperand = ci->getArgOperand(0);
						LOG_DEBUG() << "pushing arg: " << PRINT_VALUE(*sizeOperand) << "\n";
						args.push_back(sizeOperand);

						args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID
						
						inst_calls_end.addCallInst(i,"logMalloc",args);
						args.clear();
					}

					// calls to free get logFree(addr) call
					else if(called_func && called_func->getName() == "free") {
						LOG_DEBUG() << "inst is a call to free\n";

						// Insert address (first arg of call ist)
						// If op1 isn't pointing to an 8-bit int then we need to cast it to one for use.
						if(isNBitIntPointer(ci->getArgOperand(0),8)) {
							args.push_back(ci->getArgOperand(0));
						}
						else {
							args.push_back(CastInst::CreatePointerCast(ci->getArgOperand(0),types.pi8(),"free_arg_recast",blk->getTerminator()));
						}


						inst_calls_end.addCallInst(i,"logFree",args);
						args.clear();
					}

					// handle calls to realloc
					else if(called_func && called_func->getName() == "realloc") {
						LOG_DEBUG() << "isnt is  call to realloc\n";

						// Insert old addr (arg 0 of func).
						// Just like for free, we need to make sure this has type i8*
						if(isNBitIntPointer(ci->getArgOperand(0),8)) {
							args.push_back(ci->getArgOperand(0));
						}
						else {
							args.push_back(CastInst::CreatePointerCast(ci->getArgOperand(0),types.pi8(),"realloc_arg_recast",blk->getTerminator()));
						}

						// insert new addr (return val of callinst)
						args.push_back(ci);

						// insert size (arg 1 of function)
						args.push_back(ci->getArgOperand(1));

						args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

						inst_calls_end.addCallInst(i,"logRealloc",args);
						args.clear();
					}

					else if(called_func && called_func->getName() == "fscanf") {
						LOG_DEBUG() << "inst is call to fscanf\n";

						// for each of the values we are writing to, we'll use logStoreInstConst since it will be an address being passed to fscanf
						for(unsigned idx = 2; idx < ci->getNumArgOperands(); ++idx) {
							// we need to make sure this has type i8*
							if(isNBitIntPointer(ci->getArgOperand(idx),8)) {
								args.push_back(ci->getArgOperand(idx));
							}
							else {
								args.push_back(CastInst::CreatePointerCast(ci->getArgOperand(idx),types.pi8(),"fscanf_arg_recast",blk->getTerminator()));
							}

							inst_calls_end.addCallInst(i,"logStoreInstConst",args);
							args.clear();
						}
					}

					else if(called_func && uninstrumented_func_cost_map.find(called_func->getName()) != uninstrumented_func_cost_map.end()) {
						LOG_DEBUG() << "inst is call to ''library'' function: " << called_func->getName() << "\n";

						// cost of the lib function
						args.push_back(ConstantInt::get(types.i32(),uninstrumented_func_cost_map[called_func->getName()]));

						// number of ops (stupid varargs implementation)
						unsigned int num_func_args = 0;

						args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

						// as long at is isn't a constant, we are going to use it as an input to logLibraryCall
						for(unsigned int q = 0; q < ci->getNumArgOperands(); ++q) {
							Value* operand = ci->getArgOperand(q);

							if(!isa<ConstantInt>(operand) && !isa<ConstantFP>(operand)) {
								num_func_args++;
							}
						}

						args.push_back(ConstantInt::get(types.i32(),num_func_args));

						// go through and push on pointers to all the non-const inputs to the function
						for(unsigned int q = 0; q < ci->getNumArgOperands(); ++q) {
							Value* operand = ci->getArgOperand(q);


							if(!isa<ConstantInt>(operand) && !isa<ConstantFP>(operand)) {
								LOG_DEBUG() << "adding argument: " << q << "\n";
								args.push_back(ConstantInt::get(types.i32(),inst_to_id[ci->getArgOperand(q)])); // dest ID
							}
							else {
								LOG_DEBUG() << "skipping cosntant argument: " << q << "\n";
							}
						}

						inst_calls_end.addCallInst(i,"logLibraryCall",args);

						args.clear();
					}
				} // end callinst

				else if((ii = dyn_cast<InvokeInst>(i)))
				{
					static int invoke_id = 0;
					ConstantInt* invoke_id_const = ConstantInt::get(types.i32(),invoke_id++);
					args.push_back(invoke_id_const);
					inst_calls_begin.addCallInst(i,"prepareInvoke",args);

					//The exception value is pushed on to the stack, so we are not allowed to call any other function until the
					//value has been grabbed. As a result, we must push the instrumentation call to be after these calls,
					//but before any other instrumentation calls. The following gets the first instrumentation call found.
					//We will insert before it. If we don't find one, the terminator will suffice.
					{

						BasicBlock* blk = ii->getUnwindDest();

						std::vector<Instruction*> joined_blks;
						joinBasicBlocks(blk, joined_blks);

						LOG_DEBUG() << "joined blocks contents of " << blk->getName() << ":" << "\n";
						foreach(Instruction* inst, joined_blks)
							LOG_DEBUG() << PRINT_VALUE(*inst) << "\n";
						LOG_DEBUG() << "\n";

						//TODO: Need to handle if we put something in the front. If we do this, ours needs to go into the front instead of the back.
						Instruction* insert_before = joined_blks.back();
						foreach(Instruction* inst, joined_blks) {

							if(willInstrument(inst)) {
								insert_before = inst;
								break;
							}
						}

						LOG_DEBUG() << "inserting invokeThrew before " << PRINT_VALUE(*insert_before) << "\n";

						inst_calls_end.addCallInstBefore(insert_before, "invokeThrew", args);
					}

					// insert invokeOkay for this invoke instruction
					{
						BasicBlock* blk = ii->getNormalDest();
						Instruction* insert_before = blk->getFirstNonPHI();
						inst_calls_begin.addCallInstBefore(insert_before, "invokeOkay", args);
					}

					args.clear();
					instrumentCall(ii, inst_to_id, inst_calls_begin, bb_call_idx++);
				} // end invokeinst

				// store is either logStoreInst or logStoreInstConst (if storing a constant)
				else if(isa<StoreInst>(i)) {
					LOG_DEBUG() << "inst is a store inst\n";

					StoreInst* si = cast<StoreInst>(i);

					// first we get a ptr to the source (if it's not a constant)
					if(!isa<Constant>(si->getOperand(0))) {
						args.push_back(ConstantInt::get(types.i32(),inst_to_id[si->getOperand(0)])); // src ID
					}

					// the dest is already in ptr form so we simply use that
					args.push_back(CastInst::CreatePointerCast(si->getPointerOperand(),types.pi8(),"inst_arg_ptr",blk->getTerminator())); // dest addr

					if(isa<Constant>(si->getOperand(0))) {
						inst_calls_end.addCallInst(i,"logStoreInstConst",args);
					}
					else {
						inst_calls_end.addCallInst(i,"logStoreInst",args);
					}

					args.clear();
				} // end storeinst

				// look at loads to see if there is a dependence
				else if(isa<LoadInst>(i)) {
					LoadInst* li = cast<LoadInst>(i);

					/*
					//std::vector<Value*> ptr_deps; // these are the ptrs we will add as dependences at the end
					std::vector<unsigned int> ptr_deps; // these are the ptrs we will add as dependences at the end

					if(isa<GetElementPtrInst>(li->getPointerOperand())) {
						GetElementPtrInst* gepi = cast<GetElementPtrInst>(li->getPointerOperand());

						// find all non-constant ops and add them to a list for later use
						for(User::op_iterator gepi_op = gepi->idx_begin(), gepi_ops_end = gepi->idx_end(); gepi_op != gepi_ops_end; ++gepi_op) {
							if(!isa<Constant>(gepi_op)) {
								//LOG_DEBUG() << "getelementptr " << gepi->getName() << " depends on " << gepi_op->get()->getName() << "\n";
								ptr_deps.push_back(inst_to_id[*gepi_op]);
							}
						}
					}

					// find all the places that this load is used
					for(Value::use_iterator li_use = li->use_begin(), li_use_end = li->use_end(); li_use != li_use_end; ++li_use) {
						// now add a call to logInductionVarDependence for all ptr_deps

						// if this is used in a phi node, we need to insert the call to logAssignment now so the correct ordering of logInductVarDep() THEN logAssignment() is maintained
						if(PHINode* phi = dyn_cast<PHINode>(li_use)) {
							LOG_DEBUG() << "load used by phi node " << phi->getName() << "\n";

							BasicBlock* ld_bb = i->getParent();
							LOG_DEBUG() << "looking for incoming edge from " << ld_bb->getName() << "...\n";

							bool found_incoming_block = false;
							unsigned int phi_location = 0;
							for(unsigned int n = 0; n < phi->getNumIncomingValues(); ++n) {
								// if this value comes from the same block as our load, then we are good to go
								if(phi->getIncomingValue(n) == i) {
									LOG_DEBUG() << "... found incoming edge. Inserting calls to logInductionVariable() and logAssignment() in this block\n";
									phinode_completions[phi].insert(n); // mark this so when we process the phinode later, we don't insert another call to logAssignment
									phi_location = n;
									found_incoming_block = true;
									break;
								}
							}

							// big problem is we have a phi node without an incoming edge from the blk that has this load... why would this happen?
							if(!found_incoming_block) {
								LOG_DEBUG() << "ERROR: could not find edge to that phinode from this block!\n";
								LOG_DEBUG().close();
								assert(0);
								return;
							}
							else {

								// add in the calls to logInductionVarDep() before the block terminator
								for(std::vector<unsigned int>::iterator ptr_it = ptr_deps.begin(), ptr_it_end = ptr_deps.end(); ptr_it != ptr_it_end; ++ptr_it) {
									args.push_back(ConstantInt::get(types.i32(),*ptr_it)); // ind var ID

									// TODO: FIXME
									instrumentation_calls.insert(CallInst::Create(inst_funcs["logInductionVarDependence"], args.begin(), args.end(), "", ld_bb->getTerminator()));

									args.clear();
								}

								// now add in the call to logAssignment() before the block terminator (which will be immediately after the calls to logIndVarDep()
								args.push_back(ConstantInt::get(types.i32(),inst_to_id[phi->getIncomingValue(phi_location)])); // src ID
								args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

								// TODO: FIXME
								instrumentation_calls.insert(CallInst::Create(inst_funcs["logAssignment"], args.begin(), args.end(), "", ld_bb->getTerminator()));

								args.clear();
							}
						}
						else {
							for(std::vector<unsigned int>::iterator ptr_it = ptr_deps.begin(), ptr_it_end = ptr_deps.end(); ptr_it != ptr_it_end; ++ptr_it) {
								args.push_back(ConstantInt::get(types.i32(),*ptr_it)); // ind var ID
								// XXX FIXME: potential ordering problem here
								instrumentation_calls.insert(CallInst::Create(inst_funcs["logInductionVarDependence"], args.begin(), args.end(), "", dyn_cast<Instruction>(*li_use)));

								args.clear();
							}
						}
					}

					ptr_deps.clear();
					*/

					// now we create an assignment from the load's src addr to the load's ID

					// the mem loc is already in ptr form so we simply use that
					args.push_back(CastInst::CreatePointerCast(li->getPointerOperand(),types.pi8(),"inst_arg_ptr",i)); // src addr
					args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

					inst_calls_end.addCallInst(i,"logLoadInst",args);

					args.clear();
				}


				else if(isa<ReturnInst>(i)) {
					ReturnInst* ri = cast<ReturnInst>(i);
					LOG_DEBUG() << "inst is a return\n";

					if(returnsRealValue(blk->getParent()) // make sure this returns a non-pointer
					  && ri->getNumOperands() != 0) { // and that it isn't returning void
						if(isa<Constant>(ri->getReturnValue(0))) {
							inst_calls_end.addCallInst(i,"logFuncReturnConst",args);
							LOG_DEBUG() << "returning const value\n";
						}
						else {
							args.push_back(ConstantInt::get(types.i32(),inst_to_id[ri->getReturnValue(0)])); // src ID

							inst_calls_end.addCallInst(i,"logFuncReturn",args);
							LOG_DEBUG() << "returning non-const value\n";
						}
					}
					else {
						LOG_DEBUG() << "void or pointer return not logged\n";
					}

					args.clear();
				} // end returninst

			} // end basic blk iterator (instructions)
		}

		void instrumentPHINodes(BasicBlock* blk, std::set<PHINode*>& canon_indvs,std::map<Value*, unsigned int>& inst_to_id, InstrumentationCalls& inst_calls_begin, InstrumentationCalls& inst_calls_end) {
			LLVMTypes types(blk->getContext());
			LoopInfo &LI = getAnalysis<LoopInfo>(*blk->getParent());

			std::vector<Value*> args; // holds arguments passed to instrumentation functions

			for (BasicBlock::iterator i = blk->begin(), inst_end = blk->getFirstNonPHI(); i != inst_end; ++i) {
				PHINode* phi = dyn_cast<PHINode>(i);

				assert(phi && "expected phi node but didn't get one!");

				// We'll deal with induction variables later
				if(canon_indvs.find(phi) == canon_indvs.end()) {
					LOG_DEBUG() << "processing phi node: " << PRINT_VALUE(*i) << "\n";

					unsigned int dest_id = inst_to_id[i];
					args.push_back(ConstantInt::get(types.i32(),dest_id)); // dest ID

					PHINode* incoming_val_id = PHINode::Create(types.i32(),"phi-incoming-val-id",phi);

					unsigned int num_in = phi->getNumIncomingValues();
					for(unsigned int i = 0; i < num_in; i++) {

						BasicBlock* incoming_block = phi->getIncomingBlock(i);
						Value* incoming_val = phi->getIncomingValue(i);

						unsigned int incoming_id = 0; // default ID to 0 (i.e. a constant)

						// if incoming val isn't a constant, we grab the it's ID
						if(!isa<Constant>(incoming_val)) {
							incoming_id = inst_to_id[incoming_val];
						}

						incoming_val_id->addIncoming(ConstantInt::get(types.i32(),incoming_id), incoming_block);
					}

					args.push_back(incoming_val_id); // ID for incoming value

					std::set<BasicBlock*> controllers;
					getControllingBlocks(phi, controllers);

					IsPredecessor is_predecessor(blk);
					DominatorTree &dt = getAnalysis<DominatorTree>(*blk->getParent());

					std::vector<Value*> control_deps;

					for(std::set<BasicBlock*>::iterator it = controllers.begin(), end = controllers.end(); it != end; it++) {
						LOG_DEBUG() << "controller to " << blk->getName() << ": " << (*it)->getName() << "\n";
						
						PHINode* incoming_condition_addr = PHINode::Create(types.i32(),"phi-incoming-condition",phi);

						std::map<BasicBlock*, Value*> incoming_value_addrs;

						unsigned int incoming_id;
						bool all_zeros = true;

						// check all preds of current basic block
						for(unsigned int i = 0; i < num_in; i++) {
							BasicBlock* incoming_block = phi->getIncomingBlock(i);

							// If this controller dominates incoming block, get the id of condition at end of pred
							if(dt.dominates(*it, incoming_block)) {
								LOG_DEBUG() << (*it)->getName() << " dominates " << incoming_block->getName() << "\n";

								incoming_id = getConditionIdOfBlock(*it, inst_to_id);

							} 
							// doesn't dominate so condition key is 0
							else {
								LOG_DEBUG() << (*it)->getName() << " does NOT dominate " << incoming_block->getName() << "\n";
								incoming_id = 0;
							}

							if(incoming_id != 0) all_zeros = false;

							incoming_condition_addr->addIncoming(ConstantInt::get(types.i32(),incoming_id), incoming_block);
						}
						
						if(all_zeros) {
							LOG_DEBUG() << "predecessor has all 0 IDs. Erasing...\n";
							incoming_condition_addr->eraseFromParent();
						}
						else {
							control_deps.push_back(incoming_condition_addr);
						}
					}

					unsigned int num_control_deps = control_deps.size();

					// In order to avoid using a var arg function for LogPhiNode, we break this up into chunks
					// of 4 (logPhiNode4CD) plus remaining (logPhiNodeXCD, where X is 1,2, or 3).
					if(num_control_deps <= 4 && num_control_deps > 0) {
						// tack the control deps on to arg list
						args.insert(args.end(),control_deps.begin(),control_deps.end());

						std::stringstream ss;
						ss << "logPhiNode" << num_control_deps << "CD";

						// insert call to appropriate logging function at begining of blk
						inst_calls_begin.addCallInstBefore(blk->getFirstNonPHI(),ss.str(),args);
					}
					else if(num_control_deps > 0) {
						// push the first 4 control deps on and create call to logPhiNode4CD
						for(unsigned j = 0; j < 4; ++j) {
							args.push_back(control_deps[j]);
						}

						inst_calls_begin.addCallInstBefore(blk->getFirstNonPHI(),"logPhiNode4CD",args);

						// save dest because we'll need it for each call to log4CDToPhiNode
						Value* dest_val = args[0];

						args.clear();

						// keep constructing log4CDToPhiNode calls until we run out of control deps
						unsigned int num_args_added = 0;

						for(unsigned dep_index = 4; dep_index < num_control_deps; ++dep_index) {
							// check to see if we need to reset args
							if(num_args_added == 0) {
								args.clear();
								args.push_back(dest_val);
							}

							args.push_back(control_deps[dep_index]);

							num_args_added = (num_args_added+1)%4;

							// got a full set of args so let's create the func call
							if(num_args_added == 0) {
								inst_calls_begin.addCallInstBefore(blk->getFirstNonPHI(),"log4CDToPhiNode",args);
							}
						}

						// if we didn't have enough control deps to complete log4CDToPhiNode then we repeat an arbitrary control dep (e.g. the first one) to
						// get the required number of args
						if(num_args_added != 0) {
							Constant* padding = ConstantInt::get(types.i32(), 0); // 0 should indicate this is padding
							do {
								args.push_back(padding);
								num_args_added++;
							} while(num_args_added < 4);

							inst_calls_begin.addCallInstBefore(blk->getFirstNonPHI(),"log4CDToPhiNode",args);
						}
					}

					args.clear();

					//add loop conditions that occur after the phi instruction
					if(LI.isLoopHeader(blk)) {
						Loop* loop = LI.getLoopFor(blk);
						TerminatorInst* terminator = blk->getTerminator();
						BranchInst* branch_inst;

						//only try if this is conditional
						if(isa<BranchInst>(terminator) && (branch_inst = cast<BranchInst>(terminator))->isConditional()) {
							bool is_do_loop = true;
							for(unsigned int i = 0; i < branch_inst->getNumSuccessors(); i++) {
								BasicBlock* successor = branch_inst->getSuccessor(i);
								if(std::find(loop->block_begin(), loop->block_end(), successor) == loop->block_end()) {
									is_do_loop = false;
									break;
								}
							}
							for(unsigned int i = 0; i < branch_inst->getNumSuccessors(); i++) {
								BasicBlock* successor = branch_inst->getSuccessor(i);
								if(&*blk == successor) {
									is_do_loop = true;
									break;
								}
							}

							//do..while loops need the condition appended after the loop concludes
							if(is_do_loop)
								for(unsigned int i = 0; i < branch_inst->getNumSuccessors(); i++) {
									BasicBlock* successor = branch_inst->getSuccessor(i);
									if(!is_predecessor(successor) && &*blk != successor) {
										args.push_back(ConstantInt::get(types.i32(), dest_id));                                            // dest ID
										args.push_back(ConstantInt::get(types.i32(), getConditionIdOfBlock(blk, inst_to_id)));             // condition ID

										inst_calls_begin.addCallInstBefore(successor->getFirstNonPHI(),"logPhiNodeAddCondition",args);
										args.clear();
									}
								}

							//while loops need the condition appended as soon as the header executes
							else {
								// Check to see if this is a constant value (i.e. while true)
								// TODO: check to see if this occurs
								if(!isa<ConstantInt>(branch_inst->getCondition())) { 

									args.push_back(ConstantInt::get(types.i32(), dest_id));                                            // dest ID
									args.push_back(ConstantInt::get(types.i32(), getConditionIdOfBlock(blk, inst_to_id)));             // condition ID

									Instruction* condition = dyn_cast<Instruction>(branch_inst->getCondition());
									if(!condition) {
										log.error() << "branch condition isn't an instruction: " << *branch_inst->getCondition() << "\n";
										assert(0);
									}
									assert(condition);
									inst_calls_end.addCallInstAfter(condition,"logPhiNodeAddCondition",args);
									args.clear();
								}
							}
						}
					} //end handling phi's in loops
				} // end not canonical phi
			} // end looping over phi's
		}

		void instrumentBasicBlock(BasicBlock* blk, std::set<PHINode*>& canon_indvs, std::set<Instruction*>& canon_incs, std::set<Instruction*>& red_var_ops, std::map<Value*, unsigned int>& inst_to_id, InstrumentationCalls& inst_calls_begin, InstrumentationCalls& inst_calls_end) {
			//LOG_DEBUG() << "processing BB: " << PRINT_VALUE(*blk);

			instrumentNonPHIInsts(blk,canon_incs,red_var_ops,inst_to_id,inst_calls_begin,inst_calls_end);
			instrumentPHINodes(blk,canon_indvs,inst_to_id,inst_calls_begin,inst_calls_end);
		}

		void instrumentBasicBlockIndVars(BasicBlock* blk, std::set<PHINode*>& canon_indvs, std::map<Value*,unsigned int>& inst_to_id, InstrumentationCalls& inst_calls_end) {
			LLVMTypes types(blk->getContext());

			std::vector<Value*> args;

			for (BasicBlock::iterator i = blk->begin(), inst_end = blk->getFirstNonPHI(); i != inst_end; ++i) {

				// If this a PHI that is a loop induction var, we only logAssignment when we first enter the loop so that we don't get
				// caught in a dependency cycle. For example, the induction var inc would get the time of t_init for the first iter,
				// which would then force the next iter's t_init to be one higher and so on and so forth...
				if(canon_indvs.find(cast<PHINode>(i)) != canon_indvs.end()) {
					LOG_DEBUG() << "processing canonical induction var (function: " << i->getParent()->getParent()->getName() << "): " << *i << "\n";

					PHINode* phi = cast<PHINode>(i);

					// Loop through all incoming vals of PHI and find the one that is constant (i.e. initializes the loop to 0).
					// We then insert a call to logAssignmentConst() to permanently set the time of this to whatever t_init happens
					// to be in the BB where that value comes from.
					int const_idx = -1;

					for(unsigned int n = 0; n < phi->getNumIncomingValues(); ++n) {
						if(isa<ConstantInt>(phi->getIncomingValue(n)) && const_idx == -1) {
							const_idx = n;
						}
						else if(isa<ConstantInt>(phi->getIncomingValue(n))) {
							LOG_DEBUG() << "ERROR: found multiple incoming constants to induction var phi node\n";
							log.close();
							assert(0);
						}
					}

					if(const_idx == -1) {
						LOG_DEBUG() << "ERROR: could not find constant incoming value to induction variable phi node\n";
						log.close();
						assert(0);
					}

					BasicBlock* in_blk = phi->getIncomingBlock(const_idx);

					// finally, we will insert the call to logAssignmentConst
					args.push_back(ConstantInt::get(types.i32(),inst_to_id[i])); // dest ID

					inst_calls_end.addCallInstBefore(in_blk->getTerminator(),"logInductionVar",args);

					args.clear();
				}
			}
		}

		void instrumentBasicBlockControlDeps(BasicBlock* blk, std::map<Value*,unsigned int>& inst_to_id, InstrumentationCalls& inst_calls_begin, InstrumentationCalls& inst_calls_end) {
			LLVMTypes types(blk->getContext());

			BasicBlock* controller = getControllingBlock(blk, false);

			// move on to next blk if this one doesn't have a controller
			if(controller != NULL) {
				//LOG_DEBUG() << blk->getName() << " is controlled by " << controller->getName() << ". inserting call to linkinit...\n";

				std::vector<Value*> args;

				unsigned int cond_id = 0;

				// get ptr to the condition variable

				if(BranchInst* br_inst = dyn_cast<BranchInst>(controller->getTerminator())) {
					cond_id = inst_to_id[br_inst->getCondition()];
				}
				else if(SwitchInst* sw_inst = dyn_cast<SwitchInst>(controller->getTerminator())) {
					cond_id = inst_to_id[sw_inst->getCondition()];
				}
				else if(dyn_cast<InvokeInst>(controller->getTerminator())) {
					//TODO: Get the condition of the exception that was thrown
					return;
				}
				else {
					LOG_ERROR() << "controlling terminator (" << controller->getTerminator()->getName() << ": " << PRINT_VALUE(*controller->getTerminator()) << ") is not a branch or switch. what is it???\n";
					log.close();
					assert(0);
				}

				args.push_back(ConstantInt::get(types.i32(),cond_id)); // cond ID
				inst_calls_begin.addCallInstBefore(blk->getFirstNonPHI(),"addControlDep",args);

				args.clear();

				inst_calls_end.addCallInstBefore(blk->getTerminator(),"removeControlDep",args);

				args.clear();
			}
		}

		void createIDomMap(Function* func) {
			for(Function::iterator blk = ++func->begin(), blk_end = func->end(); blk != blk_end; ++blk) {
				idom[blk] = getImmediateDominator(blk);
			}
		}

		void instrumentModule(Module &m, unsigned int &op_id) {

			std::string mod_name = m.getModuleIdentifier();
			LOG_DEBUG() << "instrumenting module: " << mod_name << "\n";


			// All the instrumentation functions we may insert.
			InstrumentationFuncManager inst_funcs(m);
			inst_funcs.initializeDefaultValues();

			LLVMTypes types(m.getContext());

			const GetTerminator get_terminator;
			const GetFirstNonPHI get_non_phi;
			InstrumentationCalls inst_calls_end(inst_funcs, get_terminator);
			InstrumentationCalls inst_calls_begin(inst_funcs, get_non_phi);

			timespec start_time, end_time;

			for (Module::iterator func = m.begin(), func_end = m.end(); func != func_end; ++func) {

				// Don't try instrumenting this if it's just a declaration or if it is external
				// but LLVM is showing the code anyway.
				if(func->isDeclaration()
				  || func->getLinkage() == GlobalValue::AvailableExternallyLinkage
				  ) {
					//LOG_DEBUG() << "ignoring external function " << func->getName() << "\n";
					continue;
				}
				// for now we don't instrument vararg functions (TODO: figure out how to support them)
				else if(func->isVarArg() && func->getName() != "MAIN__") { 
					LOG_WARN() << "Not instrumenting var arg function: " << func->getName() << "\n";
					continue;
				}

				clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&start_time);

				LOG_DEBUG() << "instrumenting function " << func->getName() << "\n";
				LOG_DEBUG() << "bb count" << func->getBasicBlockList().size() << "\n";

				std::vector<Value*> args;  // Arguments to insrumentation functions

				unsigned int curr_id = 1;
				
				/*
				foreach(BasicBlock& blk, *func)
					getBasicBlockId(&blk);
				*/

				bool isMain = func->getName().compare("main") == 0 || func->getName().compare("MAIN__") == 0;
					
				// Create ID's for all the instructions that will get their own virtual register number.
				// We do this here so that we don't have races based on which BB is instrumented first (most likely with PHI nodes)
				std::map<Value*,unsigned int> inst_to_id;
				mapInstIDs(func, inst_to_id, curr_id);

				LoopInfo &LI = getAnalysis<LoopInfo>(*func);

				// keep a set of canonical ind. variables and their increments instructions
				std::set<PHINode*> canon_indvs;
				std::set<Instruction*> canon_incs;
				std::set<Instruction*> red_var_ops;

				for(LoopInfo::iterator loop = LI.begin(), loop_end = LI.end(); loop != loop_end; ++loop) {
					addCIVIToSet(*loop,canon_indvs,canon_incs);

					getReductionVars(LI,*loop,red_var_ops);
				}
				//LOG_DEBUG() << "number of CIVIs found in " << func->getName() << ": " << canon_incs.size() << "\n";
				

				// Set up the arguments to this function. They will either be initialized to constants (if this is main) or the values will be
				// transferred from the caller function using transferAndUnlinkArg
				// Note that fortran's main is a vararg function with no formal args. The only way we will have a vararg function here
				// is if this is a fortran main, in which case we don't want to try setting up the function arguments.
				if(!func->isVarArg()) {
					setupFuncArgs(func,inst_to_id, curr_id, inst_calls_begin, isMain);
				}

				//std::map< PHINode*,std::set<unsigned int> > phinode_completions;

				createIDomMap(func);

				for (Function::iterator blk = func->begin(), blk_end = func->end(); blk != blk_end; ++blk) {
					instrumentBasicBlock(blk,canon_indvs,canon_incs,red_var_ops,inst_to_id,inst_calls_begin,inst_calls_end);
				} 

				// Phi nodes associated with canonical induction vars require that all insts that can be inserted at the back 
				// (other than removeControlDep()) have already been "inserted".
				// This requirement is satisfied now that we have done the instrumenting ops phase so we can process these special
				// phi nodes.
				for (Function::iterator blk = func->begin(), blk_end = func->end(); blk != blk_end; ++blk) {
					instrumentBasicBlockIndVars(blk,canon_indvs,inst_to_id,inst_calls_end);
				}

				// add control dep inst functions
				for (Function::iterator blk = func->begin(), blk_end = func->end(); blk != blk_end; ++blk) {
					instrumentBasicBlockControlDeps(blk,inst_to_id,inst_calls_begin,inst_calls_end);
				}

				// insert call to setupLocalTable so we know how many spots in the local table to allocate 
				// (this function will go away when we implement logFunctionEntry)
				args.push_back(ConstantInt::get(types.i32(),curr_id)); // number of virtual registers
				inst_calls_begin.addCallInstBefore(func->begin()->begin(),"setupLocalTable",args);
				args.clear();

				// add calls to BBs (as needed)
				inst_calls_begin.appendInstCallsToFunc(func);
				inst_calls_end.appendInstCallsToFunc(func);

				clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&end_time);

				timespec elapsed_time = diff(start_time,end_time);

				long elapsed_time_ms = elapsed_time.tv_nsec / 1000000;

				std::string padding;
				if(elapsed_time_ms < 10) padding = "00";
				else if(elapsed_time_ms < 100) padding = "0";
				else padding = "";


				LOG_DEBUG() << "elapsed time for instrumenting " << func->getName() << ": " << elapsed_time.tv_sec << "." << padding << elapsed_time_ms << " s\n";
			}

            foreach(InstrumentationCall& c, instrumentationCalls)
            {
                c.instrument();
            }

			//LOG_DEBUG() << "num of instrumentation calls in module " << m.getModuleIdentifier() << ": " << instrumentation_calls.size() << "\n";
		}

		void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesCFG();
			AU.addRequired<LoopInfo>();
			AU.addRequired<PostDominanceFrontier>();
			AU.addRequired<PostDominatorTree>();
			AU.addRequired<DominatorTree>();
		}

		// returns a list of instructions that are considered end points.
		// these will be either return instructions or the instruction right before an "unreachable" instruction
		std::set<Instruction*> getFunctionEndPoints(Function* func) {
			std::set<Instruction*> end_pts;

			for (Function::iterator blk = func->begin(), blk_end = func->end(); blk != blk_end; ++blk) {
				if(isa<ReturnInst>(blk->getTerminator()))
					end_pts.insert(blk->getTerminator());
				else if(isa<UnreachableInst>(blk->getTerminator())) {
					BasicBlock::iterator noreturn_call = blk->getTerminator();
					--noreturn_call;

					// we expect this to be a function with the noreturn attribute so issue a warning if it isn't
					if(!isa<CallInst>(noreturn_call))
						LOG_WARN() << "something other than a function call came before unreachable instruction.\n";

					end_pts.insert(noreturn_call);
				}
			}

			return end_pts;
		}
	};  // end of struct CriticalPath

	char CriticalPath::ID = 0;
    const std::string CriticalPath::CPP_THROW_FUNC = "__cxa_throw";
    const std::string CriticalPath::CPP_RETHROW_FUNC = "";
    const std::string CriticalPath::CPP_EH_EXCEPTION = "llvm.eh.exception";
    const std::string CriticalPath::CPP_EH_TYPE_ID = "llvm.eh.typeid.for";
    const std::string CriticalPath::CPP_EH_SELECTOR = "llvm.eh.selector";

	RegisterPass<CriticalPath> X("criticalpath", "Critical Path Instrumenter",
	  false /* Only looks at CFG */,
	  false /* Analysis Pass */);
} // end anon namespace

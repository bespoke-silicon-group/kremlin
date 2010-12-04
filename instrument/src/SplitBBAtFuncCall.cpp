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
#include <cassert>

#include "llvm/Support/CFG.h"

#include <limits.h>


using namespace llvm;

namespace {

	struct SplitBBAtFuncCall : public BasicBlockPass {
		static char ID;
		SplitBBAtFuncCall() : BasicBlockPass(ID) {}

		template <typename Callable>
		bool splitBBIfCallable(Callable* c, BasicBlock::iterator& i, BasicBlock::iterator& ie, BasicBlock* BB) {
			if(!c->getCalledFunction() // either this is a function pointer or the function name doesn't contain the string "llvm.dbg"
				|| c->getCalledFunction()->getName().find("llvm.dbg") == std::string::npos)
			{
				BasicBlock* curr_bb = i->getParent();

				// split the block at this call inst (invalidates i)
				curr_bb = curr_bb->splitBasicBlock(i, BB->getName() + "_fn" + i->getName());

				// set i to 2nd inst in new BB (i.e. first inst after fn call)
				i = curr_bb->begin();

				ie = curr_bb->end();
				assert(i != ie && "current instruction is the end already!");

				return true;
			}
			return false;
		}

		virtual bool runOnBasicBlock(BasicBlock &BB) {
			bool was_changed = false;

			BasicBlock::iterator i, ie;

			// i should start out as the instruction AFTER the first non-PHI
			i = BB.getFirstNonPHI();
			i++;

			// this is last inst in the original basic block
			ie = BB.end();

			// keep going until we reach the end of the instructions
			while(i != ie) {
				if(CallInst* ci = dyn_cast<CallInst>(i))
					was_changed |= splitBBIfCallable(ci, i, ie, &BB);
				else if(InvokeInst* ii = dyn_cast<InvokeInst>(i))
					was_changed |= splitBBIfCallable(ii, i, ie, &BB);
				i++;
			}

			return was_changed;
		}// end runOnBasicBlock(...)

		void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesCFG();
		}
	};  // end of struct SplitBBAtFuncCall

	char SplitBBAtFuncCall::ID = 0;

	RegisterPass<SplitBBAtFuncCall> X("splitbbatfunccall", "Ensures that every function call is the first non-PHI instruction in a basic block",
	  false /* Only looks at CFG? */,
	  false /* Analysis Pass? */);
} // end anon namespace

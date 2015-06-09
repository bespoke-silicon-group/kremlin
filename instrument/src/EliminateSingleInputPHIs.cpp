#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;

namespace {

	struct EliminateSingleInputPHIs : public FunctionPass {
		static char ID;
		EliminateSingleInputPHIs() : FunctionPass(ID) {}

		virtual bool runOnFunction(Function &F) {
			bool was_changed = false;

			for(Function::iterator bb = F.begin(), bbe = F.end(); bb != bbe; ++bb) {
				std::vector<PHINode*> to_erase;

				for(BasicBlock::iterator inst = bb->begin(), inst_end = bb->end(); inst != inst_end; ++inst) {
					if(isa<PHINode>(inst) && cast<PHINode>(inst)->getNumIncomingValues() == 1) {
						PHINode* phi = cast<PHINode>(inst);

						Value* incoming_val = phi->getIncomingValue(0);

						// make all users of this PHI use the incoming value instead
						phi->replaceAllUsesWith(incoming_val);

						// now obliterate this stupid thing
						to_erase.push_back(phi);

						was_changed = true;
					}
				}

				for(unsigned i = 0; i < to_erase.size(); ++i) {
					to_erase[i]->eraseFromParent();
				}
			}

			return was_changed;
		}// end runOnFunction(...)

		void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesCFG();
		}
	};  // end of struct EliminateSingleInputPHIs

	char EliminateSingleInputPHIs::ID = 0;

	RegisterPass<EliminateSingleInputPHIs> X("elimsinglephis", "Eliminates all PHIs that have only a single incoming value",
	  false /* Only looks at CFG */,
	  false /* Analysis Pass */);
} // end anon namespace

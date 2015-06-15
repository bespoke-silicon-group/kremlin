#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "PassLog.h"

using namespace llvm;

namespace {

	bool containing_function_lt(llvm::CallInst* ci1, llvm::CallInst* ci2) {
		Function* ci1_func = ci1->getParent()->getParent();
		Function* ci2_func = ci2->getParent()->getParent();

		if(ci1_func == ci2_func) {
			//log.info() << "ci1 and ci2 are in same function\n";
			BasicBlock* ci1_bb = ci1->getParent();
			BasicBlock* ci2_bb = ci2->getParent();

			// true if ci1 appears before ci2 in the bb inst list
			if(ci1_bb == ci2_bb) {
				//log.info() << "ci1 and ci2 are in same basic block\n";
				for(BasicBlock::iterator inst = ci1_bb->begin(), inst_end = ci1_bb->end(); inst != inst_end; ++inst) {
					if(cast<Instruction>(inst) == ci1) {
						return true;
					}
					else if(cast<Instruction>(inst) == ci2) {
						return false;
					}
				}

				assert(0 && "didn't find either ci1 or ci2 in the bb");
			}
			// true if ci1_bb appears before ci2_bb in the basic block list
			else {
				for(Function::iterator bb = ci1_func->begin(), bb_end = ci1_func->end(); bb != bb_end; ++bb) {
					if(cast<BasicBlock>(bb) == ci1_bb) {
						return true;
					}
					else if(cast<BasicBlock>(bb) == ci2_bb) {
						return false;
					}
				}

				assert(0 && "didn't find either c1_bb or c2_bb");
			}
		}
		else {
			return ci1_func->getName().compare(ci2_func->getName()) < 0;
		}
	}

	struct FunctionUniquify : public ModulePass {
		static char ID;

		PassLog& log;

		FunctionUniquify() : ModulePass(ID), log(PassLog::get()) {}

		virtual bool runOnModule(Module &M) {
			unsigned i = 0;
			for(Module::iterator func = M.begin(), func_end = M.end(); func != func_end; ++func) {
				if(!func->isDeclaration()
				  && func->getLinkage() != GlobalValue::AvailableExternallyLinkage
				  ) { 
					log.debug() << i << ": " << func->getName() << "\n"; i++; 
				}
			}

			bool changes_made = false;

			// if we clone a function there is a chance we will add a new callsite that will create a new duplication
			// We therefore use new_changes to indicate that a cloning happened in the last pass and therefore we need
			// do an additional pass to check for new duplicates.
			bool new_changes = false; 

			unsigned int pass_num = 0; // this is so we don't double print warnings

			do {
				log.info() << "BEGIN: pass " << pass_num << "\n";

				new_changes = false;

				std::vector<Function*> funcs_to_add;

				for(Module::iterator func = M.begin(), func_end = M.end(); func != func_end; ++func) {
					
					if(func->isDeclaration()
				  	  || func->getLinkage() == GlobalValue::AvailableExternallyLinkage
					  ) {
						// this is potentially very bad if we have a declared function being called twice
						/*
						if(pass_num == 0 && func->getNumUses() > 1) {
							log.warn() << "Declaration of function " << func->getName() << " called multiple times in " << M.getModuleIdentifier() << "\n";
						}
						*/
					}
					else if(func->getNumUses() > 1) { 
						changes_made = true;

						std::vector<CallInst*> call_insts;
						std::vector<Function*> cloned_funcs;

						log.info() << func->getNumUses() << " uses of " << func->getName() << " found. Uniquifying calls to this function.\n";

						// Need to go through and create at least some predictable ordering for the uses. The most critical factor
						// is to make sure that the function that the callsite that does NOT get uniquified comes from is determinant.
						unsigned use_no = 0;
						for(Value::use_iterator ui = func->use_begin(), uie = func->use_end(); ui != uie; ++ui, ++use_no) {
							log.info() << "\tuse " << use_no << ": " << **ui << "\n";

							// If this user is a store or compare, it's most likely being used as a function pointer.
							// Function pointers aren't really supported so we'll warn the user when this is the case.
							if(isa<StoreInst>(*ui)
							  || isa<CmpInst>(*ui)
							  || isa<GlobalValue>(*ui)
							  ) {
								log.warn() << "use suggests function pointer usage. Function pointers are not uniquified.\n";
								continue;
							}
							else if(isa<ConstantStruct>(*ui)) { log.warn() << "ah HA!\n"; }

							assert(isa<CallInst>(*ui) && "unsupported type of user");

							CallInst* ci = cast<CallInst>(*ui);
							call_insts.push_back(cast<CallInst>(*ui));

							//log.info() << "\t\t...in function: " ci->getParent()->getParent()->getName() << "\n";
						}

						if(call_insts.size() < 2) { continue; }

						new_changes = true;

						// sort call_insts by name of containing function
						std::sort(call_insts.begin(),call_insts.end(),containing_function_lt);

						for(unsigned i = 0; i < call_insts.size(); ++i) {
							assert(cast<Function>(func) != call_insts[i]->getParent()->getParent() && "can't uniquify recursive calls");
						}

						// we aren't going to replace the first callsite so erase it now
						call_insts.erase(call_insts.begin());

						for(unsigned i = 0; i < call_insts.size(); ++i) {
							// create clone of this function
							ValueToValueMapTy VMap;

							Function *cloned_func = CloneFunction(func,VMap,true);

							// let's try to avoid tacking on additional __unquified's to function names
							if(func->getName().find("__uniquified") == std::string::npos) {
								cloned_func->setName(func->getName() + "__uniquified");
							}
							else {
								cloned_func->setName(func->getName());
							}

							// save the callinst and cloned func for later replacement
							cloned_funcs.push_back(cloned_func);

							// mark cloned func as a new one (which we will add to this module later)
							funcs_to_add.push_back(cloned_func);
						}

						// replace use of old function with cloned in all the callsites
						for(unsigned i = 0; i < call_insts.size(); ++i) {
							//log.info() << "\tUsing " << cloned_funcs[i]->getName() << "...\n";
							//log.info() << "\t\t... at call site: " << *call_insts[i];
							log.info() << "\t" << "replacing use " << i+1 << "\n"; //in function " << call_insts[i]->getParent()->getParent()->getName() << "\n";
							call_insts[i]->replaceUsesOfWith(func,cloned_funcs[i]);
						}
					}
				}

				// add new functions to this module
				for(unsigned i = 0; i < funcs_to_add.size(); ++i) {
					M.getFunctionList().push_back(funcs_to_add[i]);
				}

				pass_num++;
			} while(new_changes && pass_num <= 100);

			assert(!new_changes && "cut off point for passes reached. This is likely because there is a loop in the call graph");

			return changes_made;
		} // end runOnModule(...)
	};  // end of struct FunctionUniquify

	char FunctionUniquify::ID = 0;

	RegisterPass<FunctionUniquify> X("uniquify", "Makes sure that all functions are called from only one place, creating clones where necessary.",
	  false /* Only looks at CFG */,
	  false /* Analysis Pass */);
} // end anon namespace

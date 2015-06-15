#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/CommandLine.h"

#include "LLVMTypes.h"
#include "PassLog.h"


using namespace llvm;

namespace {
	struct RenameMain : public ModulePass {
		static char ID;

		PassLog& log;

		RenameMain() : ModulePass(ID), log(PassLog::get()) {}

		virtual bool runOnModule(Module &M) {
			bool was_changed = false;

			for(Module::iterator func = M.begin(), f_e = M.end(); func != f_e; ++func) {
				if(func->getName() == "main") {
					log.debug() << "Found main function. Renaming...\n";
					func->setName("__main");
					was_changed = true;
				}
			}

			return was_changed;
		}// end runOnModule(...)

	};  // end of struct RenameMain

	char RenameMain::ID = 0;

	RegisterPass<RenameMain> X("renamemain", "Looks for main function and renames it to __main.",
	  false /* Only looks at CFG? */,
	  false /* Analysis Pass? */);
} // end anon namespace

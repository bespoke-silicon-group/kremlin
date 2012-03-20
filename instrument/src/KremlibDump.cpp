#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Instructions.h"
#include "llvm/Instruction.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Support/CommandLine.h"
#include <map>

#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>
#include <cassert>

#include <limits.h>

#include "LLVMTypes.h"
#include "PassLog.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"


using namespace llvm;

namespace {
	static cl::opt<std::string> RegionIDMapFile("map-file",cl::desc("File containing mapping for region IDs"),cl::value_desc("filename"),cl::init("region-id-map.txt"));

	struct KremlibDump : public ModulePass {
		static char ID;

		PassLog& log;

		KremlibDump() : ModulePass(ID), log(PassLog::get()) {}

		void printCallArgs(CallInst* ci, raw_os_ostream*& os) {
			*os << "(";

			for(unsigned i = 0; i < ci->getNumArgOperands(); ++i) {
				if(i > 0) *os << ", ";

				Value* arg = ci->getArgOperand(i);
				if(ConstantInt* con = dyn_cast<ConstantInt>(arg)) {
					*os << con->getZExtValue();
				}
				else if(arg->hasName()){
					*os << arg->getName();
				}
				else {
					*os << *arg;
				}
			}

			*os << ")";
		}

		virtual bool runOnModule(Module &M) {
			LLVMTypes types(M.getContext());

			std::string dump_filename = M.getModuleIdentifier();
			dump_filename = dump_filename.substr(0,dump_filename.find_first_of("."));
			dump_filename.append(".kdump");

			std::ofstream dump_file;
			dump_file.open(dump_filename.c_str());

			log.debug() << "Writing kremlib calls to " << dump_filename << "\n";

			if(!dump_file.is_open()) {
				log.fatal() << "Could not open file: " << dump_filename << ".Aborting.\n";
				return false;
			}

			raw_os_ostream* dump_raw_os =  new raw_os_ostream(dump_file);

			std::set<std::string> kremlib_calls;
			kremlib_calls.insert("logBinaryOp");
			kremlib_calls.insert("logBinaryOpConst");
			kremlib_calls.insert("logAssignment");
			kremlib_calls.insert("logAssignmentConst");
			kremlib_calls.insert("logInsertValue");
			kremlib_calls.insert("logInsertValueConst");
			kremlib_calls.insert("logLoadInst");
			kremlib_calls.insert("logLoadInst1Src");
			kremlib_calls.insert("logLoadInst2Src");
			kremlib_calls.insert("logLoadInst3Src");
			kremlib_calls.insert("logLoadInst4Src");
			kremlib_calls.insert("logStoreInst");
			kremlib_calls.insert("logStoreInstConst");
			kremlib_calls.insert("logMalloc");
			kremlib_calls.insert("logRealloc");
			kremlib_calls.insert("logFree");
			kremlib_calls.insert("logPhiNode1CD");
			kremlib_calls.insert("logPhiNode2CD");
			kremlib_calls.insert("logPhiNode3CD");
			kremlib_calls.insert("logPhiNode4CD");
			kremlib_calls.insert("log4CDToPhiNode");
			kremlib_calls.insert("logPhiNodeAddCondition");
			kremlib_calls.insert("addControlDep");
			kremlib_calls.insert("removeControlDep");
			kremlib_calls.insert("prepareCall");
			kremlib_calls.insert("addReturnValueLink");
			kremlib_calls.insert("logFuncReturn");
			kremlib_calls.insert("logFuncReturnConst");
			kremlib_calls.insert("linkArgToLocal");
			kremlib_calls.insert("linkArgToConst");
			kremlib_calls.insert("transferAndUnlinkArg");
			kremlib_calls.insert("logLibraryCall");
			kremlib_calls.insert("logBBVisit");
			kremlib_calls.insert("logInductionVar");
			kremlib_calls.insert("logInductionVarDependence");
			kremlib_calls.insert("logReductionVar");
			kremlib_calls.insert("initProfiler");
			kremlib_calls.insert("deinitProfiler");
			kremlib_calls.insert("turnOnProfiler");
			kremlib_calls.insert("turnOffProfiler");
			kremlib_calls.insert("logRegionEntry");
			kremlib_calls.insert("logRegionExit");
			kremlib_calls.insert("setupLocalTable");

			// C++ stuff
			kremlib_calls.insert("prepareInvoke");
			kremlib_calls.insert("invokeThrew");
			kremlib_calls.insert("invokeOkay");
			kremlib_calls.insert("cppEntry");
			kremlib_calls.insert("cppExit");

			// Now we'll look for calls to logRegionEntry/Exit and replace old region ID with mapped value
			for(Module::iterator func = M.begin(), f_e = M.end(); func != f_e; ++func) {
				*dump_raw_os << "FUNCTION: ";
				if(func->hasName()) { *dump_raw_os << func->getName().str(); }
				else { *dump_raw_os << "(unnamed)"; }
				*dump_raw_os << "\n";

				for(Function::iterator bb = func->begin(), bb_e = func->end(); bb != bb_e; ++bb) {
					*dump_raw_os << "\t" << bb->getName().str() << "\n";

					for(BasicBlock::iterator inst = bb->begin(), inst_e = bb->end(); inst != inst_e; ++inst) {
						// See if this is a call to a kremlib function
						// If it is, we print it out.
						if(CallInst* ci = dyn_cast<CallInst>(inst)) {
							Function* called_func = ci->getCalledFunction();
							if(
								called_func
								&& called_func->hasName()
								&& kremlib_calls.find(called_func->getName().str()) != kremlib_calls.end()
							  )
							{
								// print out this instruction
								*dump_raw_os << "\t\t" << called_func->getName();
								printCallArgs(ci,dump_raw_os);
								*dump_raw_os << "\n";
							}
							else {
								// avoid printing this if it's an LLVM
								// debugging function
								if(!(called_func && called_func->isIntrinsic()
									&& called_func->hasName() &&
									called_func->getName().find("llvm.dbg") != std::string::npos))
								{
									
									Value* called_val = ci->getCalledValue();
									*dump_raw_os << "\t\tCALL ";
									if(called_val->hasName()) *dump_raw_os << called_val->getName();

									printCallArgs(ci,dump_raw_os);
									*dump_raw_os << "\n";
								}
							}
						}
						else if(isa<ReturnInst>(inst)) {
							*dump_raw_os << "\t\tRETURN\n";
						}
						else if(
								isa<BranchInst>(inst)
								|| isa<SwitchInst>(inst)
							   )
						{
							*dump_raw_os << "\t\t" << *inst << "\n";
						}
					}

					*dump_raw_os << "\n";
				}

				*dump_raw_os << "\n\n";
			}

			dump_file.close();
			//raw_os_ostream->close();

			return false;
		}// end runOnModule(...)

		void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesCFG();
		}
	};  // end of struct KremlibDump

	char KremlibDump::ID = 0;

	RegisterPass<KremlibDump> X("kremlib-dump", "Dumps all calls to Kremlin library (i.e. Kremlib) to file.",
	  true /* Only looks at CFG? */,
	  true /* Analysis Pass? */);
} // end anon namespace

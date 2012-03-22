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

#include <sstream> // for stringstream


using namespace llvm;

namespace {
	struct KremlibDump : public ModulePass {
		static char ID;

		PassLog& log;

		KremlibDump() : ModulePass(ID), log(PassLog::get()) {}

		std::string toHex(unsigned long long num) {
			std::stringstream stream;
			stream << "0x" << std::hex << num;
			return stream.str();
		}

		void printCallArgs(CallInst* ci, raw_os_ostream*& os, bool print_first_hex) {
			*os << "(";

			for(unsigned i = 0; i < ci->getNumArgOperands(); ++i) {
				if(i > 0) *os << ", ";

				Value* arg = ci->getArgOperand(i);
				if(ConstantInt* con = dyn_cast<ConstantInt>(arg)) {
					if (i == 0 && print_first_hex) *os << toHex(con->getZExtValue());
					else *os << con->getZExtValue();
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
			kremlib_calls.insert("_KBinary");
			kremlib_calls.insert("_KBinaryConst");
			kremlib_calls.insert("_KAssign");
			kremlib_calls.insert("_KAssignConst");
			kremlib_calls.insert("_KInsertVal");
			kremlib_calls.insert("_KInsertValConst");
			kremlib_calls.insert("_KLoad");
			kremlib_calls.insert("_KLoad1");
			kremlib_calls.insert("_KLoad2");
			kremlib_calls.insert("_KLoad3");
			kremlib_calls.insert("_KLoad4");
			kremlib_calls.insert("_KStore");
			kremlib_calls.insert("_KStoreConst");
			kremlib_calls.insert("_KMalloc");
			kremlib_calls.insert("_KRealloc");
			kremlib_calls.insert("_KFree");
			kremlib_calls.insert("_KPhi1To1");
			kremlib_calls.insert("_KPhi2To1");
			kremlib_calls.insert("_KPhi3To1");
			kremlib_calls.insert("_KPhi4To1");
			kremlib_calls.insert("_KPhiCond4To1");
			kremlib_calls.insert("_KPhiAddCond");
			kremlib_calls.insert("_KPushCDep");
			kremlib_calls.insert("_KPopCDep");
			kremlib_calls.insert("_KPrepCall");
			kremlib_calls.insert("_KLinkReturn");
			kremlib_calls.insert("_KReturn");
			kremlib_calls.insert("_KReturnConst");
			kremlib_calls.insert("_KLinkArg");
			kremlib_calls.insert("_KLinkArgConst");
			kremlib_calls.insert("_KUnlinkArg");
			kremlib_calls.insert("_KCallLib");
			kremlib_calls.insert("_KBasicBlock");
			kremlib_calls.insert("_KInduction");
			kremlib_calls.insert("_KInductionDep");
			kremlib_calls.insert("_KReduction");
			kremlib_calls.insert("_KInit");
			kremlib_calls.insert("_KDeinit");
			kremlib_calls.insert("_KTurnOn");
			kremlib_calls.insert("_KTurnOff");
			kremlib_calls.insert("_KEnterRegion");
			kremlib_calls.insert("_KExitRegion");
			kremlib_calls.insert("_KPrepRTable");

			// C++ stuff
			kremlib_calls.insert("_KPrepInvoke");
			kremlib_calls.insert("_KInvokeThrew");
			kremlib_calls.insert("_KInvokeOkay");
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
								*dump_raw_os << "\t\t" << called_func->getName();
								// if this is enter or exit region function we
								// want the first number to be printed as hex
								// (easier to debug since it's a large number
								// and region descriptor file uses hex for
								// region ID)
								bool is_entry_or_exit_func = called_func->getName().compare("_KEnterRegion") == 0 
									|| called_func->getName().compare("_KExitRegion") == 0;
								printCallArgs(ci,dump_raw_os,is_entry_or_exit_func);
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
									*dump_raw_os << "\t\tCALL: ";

									// print out, e.g. "bar =", if call inst is
									// named (so we can see where the return
									// value is being stored).
									if(ci->hasName()) *dump_raw_os << ci->getName() << " = ";

									if(called_val->hasName()) *dump_raw_os << called_val->getName();
									else *dump_raw_os << "(UNNAMED)";

									printCallArgs(ci,dump_raw_os,false);
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

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

		void processInstruction(Instruction *inst, std::set<std::string>& kremlib_calls, raw_os_ostream*& os) {
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
					*os << "\t\t" << called_func->getName();
					// If this is enter or exit region function we want the
					// first number to be printed as hex (easier to debug
					// since it's a large number and region descriptor file
					// uses hex for region ID). Same goes for callsite id in
					// KPrepCall.
					bool is_entry_or_exit_func = called_func->getName().compare("_KEnterRegion") == 0 
						|| called_func->getName().compare("_KExitRegion") == 0
						|| called_func->getName().compare("_KPrepCall") == 0;
					printCallArgs(ci,os,is_entry_or_exit_func);
					*os << "\n";
				}
				else {
					// avoid printing this if it's an LLVM
					// debugging function
					if(!(called_func && called_func->isIntrinsic()
						&& called_func->hasName() &&
						called_func->getName().find("llvm.dbg") != std::string::npos))
					{
						
						Value* called_val = ci->getCalledValue();
						*os << "\t\tCALL: ";

						// print out, e.g. "bar =", if call inst is
						// named (so we can see where the return
						// value is being stored).
						if(ci->hasName()) *os << ci->getName() << " = ";

						if(called_val->hasName()) *os << called_val->getName();
						else *os << "_UNNAMED_";

						printCallArgs(ci,os,false);
						*os << "\n";
					}
				}
			}
			else if(isa<ReturnInst>(inst)) {
				*os << "\t\tRETURN\n";
			}
			else if(BranchInst* bi = dyn_cast<BranchInst>(inst)) {
				*os << "\t\tTERMINATOR: ";
				for(unsigned i = 0; i < bi->getNumSuccessors(); ++i) {
					*os << bi->getSuccessor(i)->getName() << " ";
				}
				*os << "\n";
			}
			else if(SwitchInst* si = dyn_cast<SwitchInst>(inst)) {
				*os << "\t\tTERMINATOR: ";
				for(unsigned i = 0; i < si->getNumSuccessors(); ++i) {
					*os << si->getSuccessor(i)->getName() << " ";
				}
				*os << "\n";
			}
		}

		void processBasicBlock(BasicBlock *bb, std::set<std::string>& kremlib_calls, raw_os_ostream*& os) {
			*os << "\tBB_BEGIN: " << bb->getName().str() << "\n";

			for(BasicBlock::iterator inst = bb->begin(), inst_e = bb->end(); inst != inst_e; ++inst) {
				processInstruction(inst,kremlib_calls,os);
			}

			*os << "\tBB_END: " << bb->getName().str() << "\n";

			*os << "\n";
		}

		void processFunction(Function *func, std::set<std::string>& kremlib_calls, raw_os_ostream*& os) {
			std::string func_name;
			if(func->hasName()) { func_name = func->getName().str(); }
			else { func_name =  "(unnamed)"; }
			*os << "FUNCTION_BEGIN: " << func_name << "\n";

			log.info() << "dumping function: " << func_name << "\n";

			for(Function::iterator bb = func->begin(), bb_e = func->end(); bb != bb_e; ++bb) {
				processBasicBlock(bb,kremlib_calls,os);
			}

			*os << "FUNCTION_END: " << func_name << "\n";

			*os << "\n\n";
			os->flush();
		}

		void addKremlibCallsToSet(std::set<std::string>& kremlib_calls) {
			kremlib_calls.insert("_KBinary");
			kremlib_calls.insert("_KBinaryConst");
			kremlib_calls.insert("_KWork");
			kremlib_calls.insert("_KTimestamp");
			kremlib_calls.insert("_KTimestamp0");
			kremlib_calls.insert("_KTimestamp1");
			kremlib_calls.insert("_KTimestamp2");
			kremlib_calls.insert("_KAssign");
			kremlib_calls.insert("_KAssignConst");
			kremlib_calls.insert("_KInsertVal");
			kremlib_calls.insert("_KInsertValConst");
			kremlib_calls.insert("_KLoad");
			kremlib_calls.insert("_KLoad0");
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
			kremlib_calls.insert("_KPrintData");
			kremlib_calls.insert("_KEnterRegion");
			kremlib_calls.insert("_KExitRegion");
			kremlib_calls.insert("_KPrepRTable");

			// C++ stuff
			kremlib_calls.insert("_KPrepInvoke");
			kremlib_calls.insert("_KInvokeThrew");
			kremlib_calls.insert("_KInvokeOkay");
			kremlib_calls.insert("cppEntry");
			kremlib_calls.insert("cppExit");
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
			addKremlibCallsToSet(kremlib_calls);

			// Now we'll look for calls to logRegionEntry/Exit and replace old region ID with mapped value
			for(Module::iterator func = M.begin(), f_e = M.end(); func != f_e; ++func) {
				processFunction(func,kremlib_calls,dump_raw_os);
			}

			dump_file.close();

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

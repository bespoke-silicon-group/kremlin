#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"

#include <map>

#include <fstream>
#include <sstream>

#include "LLVMTypes.h"
#include "PassLog.h"

using namespace llvm;

namespace {
	static cl::opt<std::string> RegionIDMapFile("map-file",cl::desc("File containing mapping for region IDs"),cl::value_desc("filename"),cl::init("region-id-map.txt"));

	struct RemapIDs : public ModulePass {
		static char ID;

		PassLog& log;

		RemapIDs() : ModulePass(ID), log(PassLog::get()) {}

		virtual bool runOnModule(Module &M) {
			bool was_changed = false;

			LLVMTypes types(M.getContext());

			std::map<uint64_t,uint64_t> rid_mapping;

			// first we'll read in the file containing the region ID mapping
			std::ifstream file;
			std::string line;
			std::string error;

			file.open(RegionIDMapFile.c_str());
			log.debug() << "opening file with region ID mapping: " << RegionIDMapFile << "\n";

			if(!file.is_open()) {
				log.fatal() << "Could not open instrumented function list file: " << RegionIDMapFile << "\nAborting ...\n";
				return false;
			}

			while(!file.eof()) {
				getline(file,line);
				if(line == "") continue;

				log.debug() << "region ID mapping: " << line << "\n";


				std::istringstream iss(line);

				unsigned long long old_id, new_id;
				iss >> old_id >> new_id;

				assert(rid_mapping.find(old_id) == rid_mapping.end() && "Duplicate entry for ID");

				rid_mapping[old_id] = new_id;
			}


			// Now we'll look for calls to logRegionEntry/Exit and replace old region ID with mapped value
			for(Module::iterator func = M.begin(), f_e = M.end(); func != f_e; ++func) {
				for(Function::iterator bb = func->begin(), bb_e = func->end(); bb != bb_e; ++bb) {
					for(BasicBlock::iterator inst = bb->begin(), inst_e = bb->end(); inst != inst_e; ++inst) {
						CallInst* ci = dyn_cast<CallInst>(inst);

						// check to see if this is a call to either logRegionEntry or logRegionExit
						if(ci && ci->getCalledFunction()
						  && (ci->getCalledFunction()->getName() == "logRegionEntry" || ci->getCalledFunction()->getName() == "logRegionExit")
						  ) {
							CallSite *cs = new CallSite(ci);

							ConstantInt* old_region_id = dyn_cast<ConstantInt>(cs->getArgument(0)); // FIXME: is arg 0 the first arg or is it something silly like the name
							
							assert(old_region_id && "First arg to logRegionEntry/Exit is not a constant int");

							uint64_t old_rid = old_region_id->getZExtValue();

							// sanity check to make sure this id is in the mapping
							assert(rid_mapping.find(old_rid) != rid_mapping.end() && "Region ID not found in map!");

							uint64_t new_rid = rid_mapping[old_rid];

							log.info() << "Replacing old ID " << old_rid << " with new ID " << new_rid << " in the following inst: " << *ci;

							// finally, we'll replace old constant with the new constant
							cs->setArgument(0,ConstantInt::get(types.i64(),new_rid));

							was_changed = true;
						}
					}
				}
			}

			return was_changed;
		}// end runOnModule(...)

		void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesCFG();
		}
	};  // end of struct RemapIDs

	char RemapIDs::ID = 0;

	RegisterPass<RemapIDs> X("remappedid", "Remaps region IDs according to specified map input file.",
	  false /* Only looks at CFG? */,
	  false /* Analysis Pass? */);
} // end anon namespace

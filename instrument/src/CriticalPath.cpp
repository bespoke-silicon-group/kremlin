// XXX FIXME make sure callinst creations doesn't insert them into blocks yet

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Dominators.h"
#include "PostDominanceFrontier.h"

#include <ctime>

#include <foreach.h>
#include <boost/ptr_container/ptr_vector.hpp>
#include <iostream>
#include <fstream>

#include "PassLog.h"
#include "InstrumentationCall.h"
#include "InstrumentedCall.h"
#include "FuncAnalyses.h"
#include "analysis/ReductionVars.h"
#include "analysis/timestamp/TimestampAnalysis.h"
#include "analysis/timestamp/ConstantHandler.h"
#include "analysis/timestamp/ConstantWorkOpHandler.h"
#include "analysis/timestamp/LiveInHandler.h"
#include "analysis/ControlDependence.h"
#include "analysis/WorkAnalysis.h"
#include "TimestampPlacer.h"
#include "StoreInstHandler.h"
#include "CallableHandler.h"
#include "DynamicMemoryHandler.h"
#include "PhiHandler.h"
#include "FunctionArgsHandler.h"
#include "ReturnHandler.h"
#include "LoadHandler.h"
#include "LocalTableHandler.h"
#include "ControlDependencePlacer.h"


using namespace llvm;
using namespace boost;

static cl::opt<std::string> opCostFile("op-costs",cl::desc("File containing mapping between ops and their costs."),cl::value_desc("filename"),cl::init("__none__"));

/**
 * Runner for calculating the critical path.
 *
 * @todo logMalloc/logFree
 * @todo debug information
 */
struct CriticalPath : public ModulePass 
{
    /// For opt.
    static char ID;

    PassLog& log;

    boost::ptr_vector<InstrumentationCall> instrumentationCalls;

    CriticalPath() : ModulePass(ID), log(PassLog::get()) {}
    virtual ~CriticalPath() {}

    virtual bool runOnModule(Module &m) {
        // Instrument the module
        instrumentModule(m);

        log.close();

        return true;
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

    void instrumentModule(Module &m) {
        std::string mod_name = m.getModuleIdentifier();
        LOG_DEBUG() << "instrumenting module: " << mod_name << "\n";

		size_t starting_pt = mod_name.rfind('/');
		if(starting_pt == std::string::npos) starting_pt = 0;
		else ++starting_pt;
		std::string mod_base_name = mod_name.substr(0,mod_name.find('.', starting_pt));

		std::ofstream id_map_info; // used to write id mapping debug output
		std::string id_map_info_filename = mod_base_name + ".ids.txt";
		id_map_info.open(id_map_info_filename.c_str());

        timespec start_time, end_time;

        foreach(Function& func, m)
        {
            // Don't try instrumenting this if it's just a declaration or if it is external
            // but LLVM is showing the code anyway.
            if(func.isDeclaration()
              || func.getLinkage() == GlobalValue::AvailableExternallyLinkage
              ) {
                //LOG_DEBUG() << "ignoring external function " << func->getName() << "\n";
                continue;
            }
            // for now we don't instrument vararg functions (TODO: figure out how to support them)
            else if(func.isVarArg() && func.getName() != "MAIN__") { 
                LOG_WARN() << "Not instrumenting var arg function: " << func.getName() << "\n";
                continue;
            }
			else if (func.hasSection()
						&& (StringRef(func.getSection()) == ".text.startup"
							|| StringRef(func.getSection()) == ".text.exit")
					) {
                LOG_WARN() << "Not instrumenting startup function: " << func.getName() << "\n";
				continue;
			}
			else if (func.hasHiddenVisibility()) {
                LOG_WARN() << "Not instrumenting hidden function: " << func.getName() << "\n";
				continue;
			}

// Mac OS X doesn't support clock_gettime. The usage of clock_gettime here
// (timing stats for pass) isn't necessary so for now we'll just not do the
// timing if we detect we're on a mac.
#ifndef __MACH__
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&start_time);
#endif

            LOG_DEBUG() << "instrumenting function " << func.getName() << "\n";

            FuncAnalyses func_analyses(*this, func);

            // This probably could be better designed with something similar
            // to java beans and the crazy infrastructures build around those
            // with autowiring and configurations. See the spring web
            // framework (springsource.org).

            InstIds inst_ids;
            InductionVariables induc_vars(func_analyses.li);

            // Setup the timestamp analysis.
            TimestampAnalysis ts_analysis(func_analyses);

            ConstantHandler const_handler;
            ts_analysis.registerHandler(const_handler);

            LiveInHandler live_in_handler;
            ts_analysis.registerHandler(live_in_handler);

            // Setup the placer.
            TimestampPlacer placer(func, func_analyses, ts_analysis, inst_ids);

            ConstantWorkOpHandler const_work_op_handler(ts_analysis, placer, induc_vars);
            ts_analysis.registerHandler(const_work_op_handler);

            LoadHandler lh(placer);
            placer.registerHandler(lh);

            StoreInstHandler sih(placer);
            placer.registerHandler(sih);

            CallableHandler<CallInst> cih(placer);
			cih.addOpcode(Instruction::Call);

			std::vector<std::string> ignored;
			ignored.push_back("printf");
			ignored.push_back("fprintf");
			ignored.push_back("puts");
			ignored.push_back("scanf");
			ignored.push_back("fscanf");
			ignored.push_back("gets");
			ignored.push_back("fopen");
			ignored.push_back("fclose");
			ignored.push_back("exit");
			ignored.push_back("atoi");
			ignored.push_back("rand");

			// ignore mem alloc functions
			ignored.push_back("malloc");
			ignored.push_back("calloc");
			ignored.push_back("realloc");
			ignored.push_back("free");

			// ignore C++ exception handling functions
			ignored.push_back("__cxa_allocate_exception");
			ignored.push_back("__cxa_throw");
			ignored.push_back("__cxa_begin_catch");
			ignored.push_back("__cxa_end_catch");
			ignored.push_back("__gxx_personality_v0");
			cih.addIgnore(ignored);
            placer.registerHandler(cih);

            CallableHandler<InvokeInst> iih(placer);
			iih.addOpcode(Instruction::Invoke);
			iih.addIgnore(ignored);
            placer.registerHandler(iih);

            DynamicMemoryHandler dmh(placer);
            placer.registerHandler(dmh);

            FunctionArgsHandler func_args(placer);
            placer.registerHandler(func_args);

            ReturnHandler rh(placer);
            placer.registerHandler(rh);

            WorkAnalysis wa(placer, const_work_op_handler);
            placer.registerHandler(wa);

            placer.insertInstrumentation();

            // TODO: Ideally, we only want one round of placing!!
            placer.clearHandlers();

            PhiHandler ph(placer);
            placer.registerHandler(ph);

            placer.insertInstrumentation();
            placer.clearHandlers();

            ControlDependencePlacer cdp(placer);
            placer.registerHandler(cdp);

            placer.insertInstrumentation();
            placer.clearHandlers();

            LocalTableHandler ltable(placer, inst_ids);
            placer.registerHandler(ltable);

            placer.insertInstrumentation();

// see note about clock_gettime above
#ifndef __MACH__
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&end_time);

            timespec elapsed_time = diff(start_time,end_time);

            long elapsed_time_ms = elapsed_time.tv_nsec / 1000000;

            std::string padding;
            if(elapsed_time_ms < 10) padding = "00";
            else if(elapsed_time_ms < 100) padding = "0";
            else padding = "";


            LOG_DEBUG() << "elapsed time for instrumenting " << func.getName() 
			<< ": " << elapsed_time.tv_sec << "." << padding << elapsed_time_ms << " s\n";
#endif

			// TODO: outline id mapping printing
			InstIds::IdMap inst_to_id = inst_ids.getIdMap();

			id_map_info << "FUNC: " << func.getName().str() << "\n";
			for(InstIds::IdMap::iterator it = inst_to_id.begin(), it_end =
				inst_to_id.end(); it != it_end; ++it)
			{
				const Value *val = (*it).first;
				unsigned int val_id = (*it).second;


				const Instruction* inst = dyn_cast<Instruction>(val);
				if(inst != NULL)
				{
    				if(MDNode *n = inst->getMetadata("dbg"))      // grab debug metadata from inst
					{
						DILocation loc(n);
						id_map_info << "\t" << val_id << " : "
						  << loc.getLineNumber() << " : " 
						  << inst->getOpcodeName() << "\n";
					}
					else
					{
						id_map_info << "\t" << val_id << " : "
						  << "?? : "
						  << inst->getOpcodeName() << "\n";
					}
				}
				else
				{
					const Argument* arg = dyn_cast<Argument>(val);
					if(arg != NULL) {
						id_map_info << "\t" << val_id << ": ARG : "
						  << arg->getArgNo() << "\n";
					}
					else
					{
						id_map_info << "\t" << val_id << ": "
						  << "UNKNOWN (not an inst)\n";
					}
				}

			}

			id_map_info << "\n";
        }

		id_map_info.close();

        foreach(InstrumentationCall& c, instrumentationCalls)
            c.instrument();

        //LOG_DEBUG() << "num of instrumentation calls in module " << m.getModuleIdentifier() << ": " << instrumentation_calls.size() << "\n";
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequired<DominatorTreeWrapperPass>();
        AU.addRequired<PostDominanceFrontier>();
        AU.addRequired<ReductionVars>();
    }

};  // end of struct CriticalPath

char CriticalPath::ID = 0;

static RegisterPass<CriticalPath> X("criticalpath", "Critical Path Instrumenter",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

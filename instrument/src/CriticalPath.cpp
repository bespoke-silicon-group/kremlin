// XXX FIXME make sure callinst creations doesn't insert them into blocks yet

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/User.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"

#include <ctime>
#include <map>

#include <foreach.h>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/ref.hpp>

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
#include "CallInstHandler.h"
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

            clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&start_time);

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

            CallInstHandler cih(placer);
            placer.registerHandler(cih);
			cih.addIgnore("printf");
			cih.addIgnore("fprintf");
			cih.addIgnore("puts");
			cih.addIgnore("scanf");
			cih.addIgnore("fscanf");
			cih.addIgnore("gets");
			cih.addIgnore("fopen");
			cih.addIgnore("fclose");
			cih.addIgnore("exit");
			cih.addIgnore("atoi");

            DynamicMemoryHandler dmh(placer);
			cih.addIgnore("malloc");
			cih.addIgnore("calloc");
			cih.addIgnore("realloc");
			cih.addIgnore("free");
            placer.registerHandler(dmh);

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

            clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&end_time);

            timespec elapsed_time = diff(start_time,end_time);

            long elapsed_time_ms = elapsed_time.tv_nsec / 1000000;

            std::string padding;
            if(elapsed_time_ms < 10) padding = "00";
            else if(elapsed_time_ms < 100) padding = "0";
            else padding = "";


            LOG_DEBUG() << "elapsed time for instrumenting " << func.getName() 
			<< ": " << elapsed_time.tv_sec << "." << padding << elapsed_time_ms << " s\n";
        }

        foreach(InstrumentationCall& c, instrumentationCalls)
            c.instrument();

        //LOG_DEBUG() << "num of instrumentation calls in module " << m.getModuleIdentifier() << ": " << instrumentation_calls.size() << "\n";
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequiredTransitive<ReductionVars>();
        AU.addRequired<PostDominanceFrontier>();
        AU.addRequired<PostDominatorTree>();
        AU.addRequired<DominatorTree>();
    }

};  // end of struct CriticalPath

char CriticalPath::ID = 0;

static RegisterPass<CriticalPath> X("criticalpath", "Critical Path Instrumenter",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

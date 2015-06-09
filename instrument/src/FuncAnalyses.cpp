#include "FuncAnalyses.h"

using namespace llvm;

/**
 * Obtains references to the function passes.
 *
 * @param p     The module pass that can calculate the passes stored in this
 *              struct. This module needs to have the passes in its
 *              getAnalysisUsage method.
 * @param func  The function to get the analyses for.
 */
FuncAnalyses::FuncAnalyses(ModulePass& p, Function& func) :
    dt(p.getAnalysis<DominatorTreeWrapperPass>(func).getDomTree()),
    li(p.getAnalysis<LoopInfo>(func)),
    pdf(p.getAnalysis<PostDominanceFrontier>(func)),
    rv(p.getAnalysis<ReductionVars>(func)),
    cd(func, dt, pdf)
{
}

#ifndef FUNC_ANALYSES
#define FUNC_ANALYSES

#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include "analysis/ControlDependence.h"
#include "analysis/ReductionVars.h"

/**
 * Holds function analyses.
 */
struct FuncAnalyses
{
    FuncAnalyses(llvm::ModulePass& p, llvm::Function& func);

    llvm::DominatorTree& dt;
    llvm::LoopInfo& li;
    PostDominanceFrontier& pdf;
    ReductionVars& rv;
    ControlDependence cd;
};

#endif // FUNC_ANALYSES

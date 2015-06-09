#ifndef PHI_HANDLER_H
#define PHI_HANDLER_H

#include <llvm/IR/Instructions.h>
#include "TimestampPlacerHandler.h"
#include "TimestampPlacer.h"
#include "PassLog.h"
#include "analysis/ControlDependence.h"
#include "analysis/InductionVariables.h"

/**
 * Handles all phi nodes.
 */
//
// TODO: This really should be split into three handlers and a function should
// be applied to PHINodes to map the induction and reduction vars to seperate
// numbers. Those opcodes should be registered for those handlers.
class PhiHandler : public TimestampPlacerHandler
{
    public:
    PhiHandler(TimestampPlacer& ts_placer);
    virtual ~PhiHandler() {}

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    private:
    typedef std::map<size_t, llvm::Function*> SpecializedLogFuncs;

    llvm::PHINode& identifyIncomingValueId(llvm::PHINode& phi);
    std::vector<llvm::PHINode*>& getConditions(llvm::PHINode& phi, std::vector<llvm::PHINode*>& control_deps);
    unsigned int getConditionIdOfBlock(llvm::BasicBlock& bb);
    void handleLoops(llvm::PHINode& phi);
    void handleIndVar(llvm::PHINode& phi);
    void handleReductionVariable(llvm::PHINode& phi);

    llvm::Function* addCondFunc;
    ControlDependence& controlDependence;
    llvm::DominatorTree& dominatorTree;
    InductionVariables inductionVars;
    llvm::Function* inductionFunc;
    llvm::LoopInfo& loopInfo;
    PassLog& log;
    llvm::Function* phiLoggingFunc;
    SpecializedLogFuncs specializedPhiLoggingFuncs;
    Opcodes opcodes;
    ReductionVars& reductionVars;
    TimestampPlacer& timestampPlacer;
};

#endif // PHI_HANDLER_H

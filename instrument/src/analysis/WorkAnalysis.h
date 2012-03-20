#ifndef WORK_ANALYSIS_H
#define WORK_ANALYSIS_H

#include <llvm/BasicBlock.h>
#include <llvm/Instructions.h>
#include "analysis/timestamp/ConstantWorkOpHandler.h"
#include "TimestampBlockHandler.h"
#include "TimestampPlacer.h"

class WorkAnalysis : public TimestampBlockHandler
{
    public:
    WorkAnalysis(TimestampPlacer& ts_placer, const ConstantWorkOpHandler& work_handler);
    virtual ~WorkAnalysis() {}

    void handleBasicBlock(llvm::BasicBlock& bb);

    private:
    uint64_t getWork(llvm::BasicBlock& bb) const;

    TimestampPlacer& ts_placer;
    llvm::Function* log_func;
    const ConstantWorkOpHandler& work_handler;
};

#endif // WORK_ANALYSIS_H

#ifndef WORK_ANALYSIS_H
#define WORK_ANALYSIS_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include "analysis/timestamp/ConstantWorkOpHandler.h"
#include "TimestampBlockHandler.h"
#include "TimestampPlacer.h"

class WorkAnalysis : public TimestampBlockHandler
{
    public:
    WorkAnalysis(TimestampPlacer& timestamp_placer, const ConstantWorkOpHandler& work_handler);
    virtual ~WorkAnalysis() {}

    void handleBasicBlock(llvm::BasicBlock& bb);

    private:
    uint64_t getWork(llvm::BasicBlock& bb) const;

    TimestampPlacer& _timestampPlacer;
    llvm::Function* _instrumentationFunc;
    const ConstantWorkOpHandler& _workHandler;
};

#endif // WORK_ANALYSIS_H

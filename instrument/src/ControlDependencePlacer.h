#ifndef CONTROL_DEPENDENCE_PLACER_H
#define CONTROL_DEPENDENCE_PLACER_H

#include "TimestampBlockHandler.h"
#include "TimestampPlacer.h"
#include "PassLog.h"

/**
 * Adds {add,remove}ConrolDependence calls.
 */
class ControlDependencePlacer : public TimestampBlockHandler
{
    public:
    ControlDependencePlacer(TimestampPlacer& ts_placer);
    virtual ~ControlDependencePlacer() {}

    virtual void handleBasicBlock(llvm::BasicBlock& bb);

    private:
    llvm::Function* add_func;
    ControlDependence& cd;
    TimestampPlacer& ts_placer;
    llvm::Function* remove_func;
    PassLog& log;
};

#endif // CONTROL_DEPENDENCE_PLACER_H

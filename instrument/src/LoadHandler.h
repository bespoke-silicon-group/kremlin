#ifndef LOAD_HANDLER_H
#define LOAD_HANDLER_H

#include "TimestampPlacer.h"
#include "TimestampPlacerHandler.h"

/**
 * Handles inserting logLoadInst
 */
class LoadHandler : public TimestampPlacerHandler
{
    public:
    LoadHandler(TimestampPlacer& ts_placer);
    virtual ~LoadHandler() {}

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    private:
    typedef std::map<size_t, llvm::Function*> SpecializedFuncs;

    InductionVariables induc_vars;
    Opcodes opcodes;
    llvm::Function* log_func;
    TimestampPlacer& ts_placer;
    SpecializedFuncs specialized_funcs;
};

#endif // LOAD_HANDLER_H


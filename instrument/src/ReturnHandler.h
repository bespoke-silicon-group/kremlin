#ifndef RETURN_HANDLER_H
#define RETURN_HANDLER_H

#include "TimestampPlacer.h"
#include "TimestampPlacerHandler.h"

class ReturnHandler : public TimestampPlacerHandler
{
    public:
    ReturnHandler(TimestampPlacer& ts_placer);
    virtual ~ReturnHandler() {}

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    private:
    Opcodes opcodes;
    llvm::Function* ret_func;
    llvm::Function* ret_const_func;
    TimestampPlacer& ts_placer;
};

#endif // RETURN_HANDLER_H

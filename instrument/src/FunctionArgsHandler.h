#ifndef FUNCTION_ARGS_HANDLER_H
#define FUNCTION_ARGS_HANDLER_H

#include "TimestampPlacer.h"
#include "TimestampPlacerHandler.h"

class FunctionArgsHandler : public TimestampPlacerHandler
{
    public:
    FunctionArgsHandler(TimestampPlacer& ts_placer);
    virtual ~FunctionArgsHandler() {}

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    private:
    Opcodes opcodes;
};

#endif // FUNCTION_ARGS_HANDLER_H

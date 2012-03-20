#ifndef ALLOCA_HANDLER_H
#define ALLOCA_HANDLER_H

#include "TimestampPlacer.h"
#include "TimestampPlacerHandler.h"
#include "GlobalTableAllocator.h"

class AllocaHandler : public TimestampPlacerHandler
{
    public:
    AllocaHandler(TimestampPlacer& ts_placer, GlobalTableAllocator& allocator);
    virtual ~AllocaHandler() {}

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    private:
    GlobalTableAllocator& allocator;
    Opcodes opcodes;
    TimestampPlacer& ts_placer;
};

#endif // ALLOCA_HANDLER_H

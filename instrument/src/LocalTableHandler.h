#ifndef LOCAL_TABLE_HANDLER_H
#define LOCAL_TABLE_HANDLER_H

#include "TimestampPlacer.h"

class LocalTableHandler : public TimestampPlacerHandler
{
    public:
    LocalTableHandler(TimestampPlacer& ts_placer, InstIds& inst_ids);
    virtual ~LocalTableHandler() {}

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    private:
    Opcodes opcodes;
};

#endif // LOCAL_TABLE_HANDLER_H

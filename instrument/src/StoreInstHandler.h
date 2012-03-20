#ifndef STORE_INST_HANDLER_H
#define STORE_INST_HANDLER_H

#include "TimestampPlacerHandler.h"
#include "TimestampPlacer.h"
#include <vector>
#include "PassLog.h"

class StoreInstHandler : public TimestampPlacerHandler
{
    public:
    StoreInstHandler(TimestampPlacer& ts_placer);
    virtual ~StoreInstHandler() {}

    virtual const std::vector<unsigned int>& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    private:
    PassLog& log;
    llvm::Function* log_func;
    std::vector<unsigned int> opcodes;
    TimestampPlacer& ts_placer;
};

#endif // STORE_INST_HANDLER_H

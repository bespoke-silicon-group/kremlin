#ifndef DYNAMIC_MEMORY_HANDLER
#define DYNAMIC_MEMORY_HANDLER

#include <stdint.h>
#include <vector>
#include "TimestampPlacerHandler.h"
#include "TimestampPlacer.h"
#include "PassLog.h"

class DynamicMemoryHandler : public TimestampPlacerHandler
{
    public:
    DynamicMemoryHandler(TimestampPlacer& ts_placer);
    virtual ~DynamicMemoryHandler() {};

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    private:
	bool isNBitIntPointer(llvm::Value *val, unsigned n);
	llvm::Instruction* getNextInst(llvm::Instruction *inst);

    uint32_t call_idx;
    PassLog& log;
    llvm::Function* malloc_func;
    llvm::Function* realloc_func;
    llvm::Function* free_func;

    Opcodes opcodes;
    TimestampPlacer& ts_placer;
};

#endif // DYNAMIC_MEMORY_HANDLER

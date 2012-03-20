#ifndef GLOBAL_TABLE_ALLOCATOR
#define GLOBAL_TABLE_ALLOCATOR

#include "TimestampPlacer.h"

class GlobalTableAllocator
{
    public:
    GlobalTableAllocator(TimestampPlacer& ts_placer);
    virtual ~GlobalTableAllocator() {}

    void addAlloc(llvm::Value& ptr, llvm::Value& size, llvm::Instruction& use);
    void addFree(llvm::Value& ptr, llvm::Instruction& use);

    private:
    llvm::Function* alloc_func;
    llvm::Function* free_func;
    TimestampPlacer& ts_placer;
};

#endif // GLOBAL_TABLE_ALLOCATOR

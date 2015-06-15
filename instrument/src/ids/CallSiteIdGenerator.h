#ifndef CALL_SITE_ID_GENERATOR_H
#define CALL_SITE_ID_GENERATOR_H

#include <llvm/IR/Instructions.h>

class CallSiteIdGenerator
{
    public:
    template <typename Callable>
    static uint64_t generate(Callable* callable, uint64_t bb_call_idx);
};

#include "CallSiteIdGenerator.tcc"

#endif // CALL_SITE_ID_GENERATOR_H

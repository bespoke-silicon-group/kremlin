#include "AllocaHandler.h"
#include <boost/assign/std/vector.hpp>

#define MAX_SPECIALIZED 5

using namespace llvm;
using namespace boost::assign;

AllocaHandler::AllocaHandler(TimestampPlacer& ts_placer, GlobalTableAllocator& allocator) :
    allocator(allocator),
    ts_placer(ts_placer)
{
    opcodes += Instruction::Alloca;
}

const TimestampPlacerHandler::Opcodes& AllocaHandler::getOpcodes()
{
    return opcodes;
}

void AllocaHandler::handle(llvm::Instruction& inst)
{
}

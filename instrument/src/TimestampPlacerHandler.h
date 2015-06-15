#ifndef TIMESTAMP_PLACER_HANDLER_H
#define TIMESTAMP_PLACER_HANDLER_H

#include <vector>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>

/**
 * Interface for handlers inserting logTimestamp calls.
 */
class TimestampPlacerHandler
{
    public:

    /// The LLVM opcodes the handler cares about.
    typedef std::vector<unsigned int> Opcodes;

    virtual ~TimestampPlacerHandler() {}

    /// @return LLVM opcodes the handler cares about.
    virtual const Opcodes& getOpcodes() = 0;

    /// Handles the instruction. This will be called on every instruction that
    /// has an opcode that matches.
    ///
    /// @param inst The matching instruction.
    virtual void handle(llvm::Instruction& inst) = 0;
};

#endif // TIMESTAMP_PLACER_HANDLER_H

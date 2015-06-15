#ifndef TIMESTAMP_BLOCK_HANDLER_H
#define TIMESTAMP_BLOCK_HANDLER_H

#include <llvm/IR/BasicBlock.h>

/**
 * Interface for handlers called once per basic block.
 */
class TimestampBlockHandler
{
    public:

    /**
     * Handles a basic block.
     *
     * @param bb The basic block to handle.
     */
    virtual void handleBasicBlock(llvm::BasicBlock& bb) = 0;
};

#endif // TIMESTAMP_BLOCK_HANDLER_H

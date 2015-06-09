#ifndef LIVE_IN_HANDLER
#define LIVE_IN_HANDLER

#include <llvm/IR/Value.h>
#include "analysis/timestamp/TimestampHandler.h"
#include "analysis/timestamp/ValueClassifier.h"

class LiveInHandler : public TimestampHandler
{
    public:
    virtual ~LiveInHandler() {}

    virtual ValueClassifier::Class getTargetClass() const;
    virtual Timestamp& getTimestamp(llvm::Value* val, Timestamp& ts);
};

#endif // LIVE_IN_HANDLER

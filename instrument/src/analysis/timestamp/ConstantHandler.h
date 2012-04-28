#ifndef CONSTANT_HANDLER_H
#define CONSTANT_HANDLER_H

#include "analysis/timestamp/TimestampHandler.h"
#include "analysis/timestamp/ValueClassifier.h"

class ConstantHandler : public TimestampHandler
{
    public:
    virtual ~ConstantHandler() {}

    virtual ValueClassifier::Class getTargetClass() const;
    virtual Timestamp& getTimestamp(llvm::Value* val, Timestamp& ts);
};

#endif // CONSTANT_HANDLER_H

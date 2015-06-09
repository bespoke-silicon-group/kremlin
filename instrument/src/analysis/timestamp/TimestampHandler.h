#ifndef TIMESTAMP_HANDLER_H
#define TIMESTAMP_HANDLER_H

#include <typeinfo>
#include <llvm/IR/Value.h>
#include "analysis/timestamp/Timestamp.h"
#include "analysis/timestamp/ValueClassifier.h"

/**
 * Interface for handlers to recursively calculate timestamps.
 */
class TimestampHandler
{
    public:

    /**
     * @return the class of values this handler can accept.
     */
    virtual ValueClassifier::Class getTargetClass() const = 0;

    /**
     * This method should take the value and add the timestamp candidates to
     * the passed timestamp. Then it should return the passed timestamp.
     *
     * @param val   The value.
     * @param ts    The timestamp to add the candidates to.
     *
     * @return The passed timestamp reference.
     */
    virtual Timestamp& getTimestamp(llvm::Value* val, Timestamp& ts) = 0;
};

#endif // TIMESTAMP_HANDLER_H

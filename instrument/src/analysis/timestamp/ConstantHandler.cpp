#include "analysis/timestamp/ConstantHandler.h"

ValueClassifier::Class ConstantHandler::getTargetClass() const
{
    return ValueClassifier::CONSTANT;
}

Timestamp& ConstantHandler::getTimestamp(llvm::Value* val, Timestamp& ts)
{
    //ts.insert(NULL, 0);
    return ts;
}

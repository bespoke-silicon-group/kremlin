#include "analysis/timestamp/LiveInHandler.h"

using namespace llvm;

ValueClassifier::Class LiveInHandler::getTargetClass() const
{
    return ValueClassifier::LIVE_IN;
}

Timestamp& LiveInHandler::getTimestamp(llvm::Value* val, Timestamp& ts)
{
    ts.addCandidate(val, 0);
    return ts;
}

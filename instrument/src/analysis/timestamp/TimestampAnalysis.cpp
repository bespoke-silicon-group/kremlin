#define DEBUG_TYPE __FILE__

#include <memory>
#include <llvm/Support/Debug.h>
#include "analysis/timestamp/TimestampAnalysis.h"

using namespace llvm;
using namespace std;

/**
 * Constructs a new analysis for the function.
 */
TimestampAnalysis::TimestampAnalysis(FuncAnalyses& func_analyses) :
    classifier(func_analyses.rv),
    log(PassLog::get())
{
}

TimestampAnalysis::~TimestampAnalysis()
{
}

/**
 * @return The timestamp associated with the value. If the value cannot be
 * calculated dynamically, it is assumed to be calculated elsewhere and
 * retrievable by reading its timestamp in the virtual register table.
 */
const Timestamp& TimestampAnalysis::getTimestamp(llvm::Value* val) 
{
    Timestamps::iterator it = timestamps.find(val);
    Timestamp* ts;
    if(it == timestamps.end())
    {
        auto_ptr<Timestamp> auto_ts(ts = new Timestamp());
        getHandler(val)->getTimestamp(val, *auto_ts);
        timestamps.insert(val, auto_ts);
    }
    else
        ts = it->second;

    LOG_DEBUG() << "TS for " << *val << ": " << *ts << "\n";
    return *ts;
}

/**
 * @return The handler for the particular value.
 */
TimestampHandler* TimestampAnalysis::getHandler(llvm::Value* val) const
{
    Handlers::const_iterator found = handlers.find(classifier(val));

    if(found == handlers.end())
    {
        DEBUG(LOG_WARN() << "Failed to find handler for inst " << *val << "\n");
        return NULL;
    }
    return found->second;
}

/**
 * Adds a handler.
 * @param handler The handler to add.
 */
void TimestampAnalysis::registerHandler(TimestampHandler& handler)
{
    ValueClassifier::Class clazz = handler.getTargetClass();
    assert(handlers.find(clazz) == handlers.end() && "Already registered for this class");

    handlers.insert(std::make_pair(clazz, &handler));
}

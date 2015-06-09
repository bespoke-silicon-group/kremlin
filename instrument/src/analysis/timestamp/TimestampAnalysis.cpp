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
    _classifier(func_analyses.rv),
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
const Timestamp& TimestampAnalysis::getTimestamp(llvm::Value* input_val) 
{
    LOG_DEBUG() << "getting TS for " << *input_val << "\n";
    Timestamp* timestamp_of_val;
    Timestamps::iterator timestamp_iter = _timestamps.find(input_val);
    if(timestamp_iter == _timestamps.end())
    {
		// create a timestamp if one doesn't currently exist for input_val
        auto_ptr<Timestamp> timestamp_ptr_of_val(timestamp_of_val = new Timestamp());
		TimestampHandler* h = getHandler(input_val);
		assert(h != NULL && "Could not get handler");
        h->getTimestamp(input_val, *timestamp_ptr_of_val);
        _timestamps.insert(input_val, timestamp_ptr_of_val);
    }
    else
	{
		// grab existing timestamp
        timestamp_of_val = timestamp_iter->second;
	}

    LOG_DEBUG() << "\tTS for " << *input_val << ": " << *timestamp_of_val << "\n";
	PassLog::get().debug().flush();
    return *timestamp_of_val;
}

/**
 * @return The handler for the particular value.
 */
TimestampHandler* TimestampAnalysis::getHandler(llvm::Value* input_val) const
{
    Handlers::const_iterator handler_iter = _handlers.find(_classifier(input_val));

    if(handler_iter == _handlers.end())
    {
        LOG_WARN() << "Failed to find handler for inst " << *input_val << "\n";
        return NULL;
    }
    return handler_iter->second;
}

/**
 * Adds a handler.
 * @param handler The handler to add.
 */
void TimestampAnalysis::registerHandler(TimestampHandler& handler)
{
    ValueClassifier::Class clazz = handler.getTargetClass();
    assert(_handlers.find(clazz) == _handlers.end() && "Already registered for this class");

    _handlers.insert(std::make_pair(clazz, &handler));
}

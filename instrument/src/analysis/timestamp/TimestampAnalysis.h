#ifndef TIMESTAMP_ANALYSIS_H
#define TIMESTAMP_ANALYSIS_H

#include <boost/ptr_container/ptr_map.hpp>

#include <llvm/Pass.h>
#include <llvm/IR/Instruction.h>

#include "analysis/timestamp/Timestamp.h"
#include "analysis/timestamp/TimestampHandler.h"
#include "analysis/timestamp/ValueClassifier.h"
#include "FuncAnalyses.h"
#include "PassLog.h"

/**
 * Handles recursively calculating timestamps.
 *
 * This does not handle any timestamps that cannot be folded statically (e.g.
 * phi nodes, function calls).
 */
class TimestampAnalysis
{
    public:

    /// The timestamps for values.
    typedef boost::ptr_map<llvm::Value*, Timestamp> Timestamps;

    /// The handlers for classes of instructions.
    typedef std::map<ValueClassifier::Class, TimestampHandler*> Handlers;

    TimestampAnalysis(FuncAnalyses& func_analyses);
    virtual ~TimestampAnalysis();

    const Timestamp& getTimestamp(llvm::Value* input_val);
    void registerHandler(TimestampHandler& handler);

    private:
    TimestampHandler* getHandler(llvm::Value* input_val) const;

    ValueClassifier _classifier;
    PassLog& log;
    Timestamps _timestamps;
    Handlers _handlers;
};

#endif // TIMESTAMP_ANALYSIS_H

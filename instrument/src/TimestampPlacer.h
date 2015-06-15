#ifndef TIMESTAMP_PLACER_H
#define TIMESTAMP_PLACER_H

#include <boost/signals2/signal.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/scoped_ptr.hpp>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <set>
#include "Placer.h"
#include "FuncAnalyses.h"
#include "TimestampPlacerHandler.h"
#include "TimestampBlockHandler.h"
#include "analysis/timestamp/TimestampAnalysis.h"
#include "ids/InstIds.h"
#include "PassLog.h"

/**
 * Instruments a function with all of the dynamic calls to log timestamps.
 */
class TimestampPlacer
{
    public:
    TimestampPlacer(llvm::Function& func, FuncAnalyses& analyses, TimestampAnalysis& timestamp_analysis, InstIds& inst_ids);

    llvm::Function& getFunc();
    FuncAnalyses& getAnalyses();

    void registerHandler(TimestampPlacerHandler& handler);
    void registerHandler(TimestampBlockHandler& handler);
    void clearHandlers();

    unsigned int getId(const llvm::Value& inst);

    /// Get the timestamp of a value.
    const Timestamp& getTimestamp(llvm::Value& value);

    void constrainInstPlacement(llvm::Instruction& inst, llvm::Instruction& dependency);
    void constrainInstPlacement(llvm::Instruction& inst, const std::set<llvm::Instruction*> dependencies);

    /// requires that a value's timestamp be present before the user needs.
    llvm::Instruction& requireValTimestampBeforeUser(llvm::Value& value, llvm::Instruction& user);

    void insertInstrumentation();
    
    private:
    struct PlacedTimestamp
    {
        PlacedTimestamp(const Timestamp& ts, llvm::CallInst* ci);

        llvm::CallInst* ci;
        const Timestamp& ts;
    };

    typedef boost::ptr_map<llvm::Value*, PlacedTimestamp> Timestamps;
    typedef boost::signals2::signal<void(llvm::Instruction&)> Signal;
    typedef boost::ptr_map<unsigned int, Signal> Signals;
    typedef boost::signals2::signal<void(llvm::BasicBlock&)> BasicBlockSignal;

	PassLog& log;
    FuncAnalyses& _analyses;
    boost::scoped_ptr<BasicBlockSignal> _basicBlockSignal;
    llvm::Function& _func;
    InstIds& _instIds;
    Placer _placer;
    Signals _signals;
    Timestamps timestamps;
    TimestampAnalysis& _timestampAnalysis;
};

#endif // TIMESTAMP_PLACER_H

#include <boost/bind.hpp>
#include <boost/assign/std/set.hpp>
#include <foreach.h>
#include "TimestampPlacer.h"
#include "analysis/timestamp/KInstructionToLogFunctionConverter.h"

using namespace boost;
using namespace boost::assign;
using namespace llvm;
using namespace std;

/**
 * Saves the call instruction associated with the timestamp.
 *
 * @param ts The timestamp.
 * @param ci The call instruction to the instrumentation call.
 */
TimestampPlacer::PlacedTimestamp::PlacedTimestamp(const Timestamp& ts, llvm::CallInst* ci) :
    ci(ci),
    ts(ts)
{
}

/**
 * Constructs a new placer for the function.
 *
 * @param func          The function to instrument.
 * @param analyses      The analyses for the function.
 * @param ts_analysis   The analysis that recursively calculates timestamps.
 * @param inst_ids      The map of instructions to their virtual register table
 *                      index.
 */
TimestampPlacer::TimestampPlacer(llvm::Function& func, FuncAnalyses& analyses, TimestampAnalysis& ts_analysis, InstIds& inst_ids) :
    analyses(analyses),
    func(func),
    inst_ids(inst_ids),
    placer(func, analyses.dt),
    ts_analysis(ts_analysis)
{
}

/**
 * Adds an instruction to be placed. The inst will be placed before the user.
 *
 * @param inst  The instruction to place.
 * @param user  The user of the instruction.
 */
void TimestampPlacer::add(llvm::Instruction& inst, llvm::Instruction& user)
{
    set<Instruction*> users;
    users += &user;
    placer.add(inst, users);
}

/**
 * Adds an instruction to be placed. The inst will be placed before the users.
 *
 * @param inst  The instruction to place.
 * @param users The set of all the users of the instruction.
 */
void TimestampPlacer::add(llvm::Instruction& inst, const std::set<llvm::Instruction*> users)
{
    placer.add(inst, users);
}

/**
 * Clears out all of the registered handlers.
 */
void TimestampPlacer::clearHandlers()
{
    signals.clear();
    bb_sig.reset();
}

/**
 * @return The analyses.
 */
FuncAnalyses& TimestampPlacer::getAnalyses()
{
    return analyses;
}

/**
 * @return The function.
 */
llvm::Function& TimestampPlacer::getFunc()
{
    return func;
}

/**
 * @return the Id associated with the value.
 */
unsigned int TimestampPlacer::getId(const llvm::Value& inst)
{
    return inst_ids.getId(inst);
}

/**
 * Adds a handler for instructions.
 *
 * @param handler The handler to add.
 */
void TimestampPlacer::registerHandler(TimestampPlacerHandler& handler)
{
    // Register opcodes this handler cares about
    foreach(unsigned int opcode, handler.getOpcodes())
    {
        Signals::iterator it = signals.find(opcode);
        Signal* sig;
        if(it == signals.end())
            sig = signals.insert(opcode, new Signal()).first->second;
        else
            sig = it->second;

        sig->connect(bind(&TimestampPlacerHandler::handle, &handler, _1));
    }
}

/**
 * Adds a handler for basic blocks.
 *
 * @param handler The handler to add.
 */
void TimestampPlacer::registerHandler(TimestampBlockHandler& handler)
{
    if(!bb_sig)
        bb_sig.reset(new BasicBlockSignal());

    bb_sig->connect(bind(&TimestampBlockHandler::handleBasicBlock, &handler, _1));
}

/**
 * Requests that the timestamp for the particular value exists before the
 * user.
 *
 * @param value The value associated with the needed timestamp.
 * @param user  The user that needs the calculated timestamp.
 *
 * @return The call instruction to calculate the timestamp.
 */

// XXX: Design hack. Refactor away please!
// This should really only provide the interface of requesting that timestamps
// be present before some instruction or getting the timestamp of some value,
// but not necessarily making it available. TimestampAnalysis should
// recursively call getTimestamp for instructions that can be determined
// statically. When the timestamp must be computed at runtime, a call to
// requestTimestamp should be placed. This will cause timestamps to be
// completely lazy and only computed when needed.
llvm::Instruction& TimestampPlacer::requestTimestamp(llvm::Value& value, llvm::Instruction& user)
{
    Timestamps::iterator it = timestamps.find(&value);
    CallInst* ci;
    if(it == timestamps.end())
    {
        const Timestamp& ts = ts_analysis.getTimestamp(&value);
        InstructionToLogFunctionConverter converter(*func.getParent(), inst_ids);
        ci = converter(&value, ts);
        Value* pval = &value;
        timestamps.insert(pval, new PlacedTimestamp(ts, ci));

        // Bind the instruction to the terminator of the block it was
        // generated. This preseves control dependencies.
        Instruction* inst = dyn_cast<Instruction>(pval);
        if(inst)
            add(*ci, *inst->getParent()->getTerminator());
    }
    else
        ci = it->second->ci;

    add(*ci, user);
    return *ci;
}

/**
 * Instruments the function.
 */
void TimestampPlacer::run()
{
    foreach(BasicBlock& bb, func)
    {
        if(bb_sig)
            (*bb_sig)(bb);

        foreach(Instruction& inst, bb)
        {
            Signals::iterator it = signals.find(inst.getOpcode());
            if(it != signals.end())

                // Call the signal with the instruction.
                (*it->second)(inst);
        }
    }

    placer.place();
}

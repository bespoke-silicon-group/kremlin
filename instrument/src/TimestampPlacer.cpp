#include <boost/bind.hpp>
#include <foreach.h>
#include "TimestampPlacer.h"
#include "analysis/timestamp/KInstructionToLogFunctionConverter.h"

using namespace boost;
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
 * @param timestamp_analysis   The analysis that recursively calculates timestamps.
 * @param inst_ids      The map of instructions to their virtual register table
 *                      index.
 */
TimestampPlacer::TimestampPlacer(llvm::Function& func, FuncAnalyses& analyses, TimestampAnalysis& timestamp_analysis, InstIds& inst_ids) :
	log(PassLog::get()),
    _analyses(analyses),
    _func(func),
    _instIds(inst_ids),
    _placer(func, analyses.dt),
    _timestampAnalysis(timestamp_analysis)
{
}

/**
 * Adds an instruction to be placed. The inst will be placed before the user.
 *
 * @param inst  The instruction to place.
 * @param user  The user of the instruction.
 */
void TimestampPlacer::constrainInstPlacement(llvm::Instruction& inst, llvm::Instruction& user)
{
    set<Instruction*> users;
    users.insert(&user);
    _placer.addInstForPlacement(inst, users);
}

/**
 * Adds an instruction to be placed. The inst will be placed before the users.
 *
 * @param inst  The instruction to place.
 * @param users The set of all the users of the instruction.
 */
void TimestampPlacer::constrainInstPlacement(llvm::Instruction& inst, const std::set<llvm::Instruction*> users)
{
    _placer.addInstForPlacement(inst, users);
}

/**
 * Clears out all of the registered handlers.
 */
void TimestampPlacer::clearHandlers()
{
    _signals.clear();
    _basicBlockSignal.reset();
}

/**
 * @return The analyses.
 */
FuncAnalyses& TimestampPlacer::getAnalyses()
{
    return _analyses;
}

/**
 * @return The function.
 */
llvm::Function& TimestampPlacer::getFunc()
{
    return _func;
}

/**
 * @return the Id associated with the value.
 */
unsigned int TimestampPlacer::getId(const llvm::Value& inst)
{
    return _instIds.getId(inst);
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
        Signal* sig;
        Signals::iterator signal_iter = _signals.find(opcode);
        if(signal_iter == _signals.end())
            sig = _signals.insert(opcode, new Signal()).first->second;
        else
            sig = signal_iter->second;

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
    if(!_basicBlockSignal)
        _basicBlockSignal.reset(new BasicBlockSignal());

    _basicBlockSignal->connect(bind(&TimestampBlockHandler::handleBasicBlock, &handler, _1));
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

// TODO: Design hack. Refactor away please!
// This should really only provide the interface of requesting that timestamps
// be present before some instruction or getting the timestamp of some value,
// but not necessarily making it available. TimestampAnalysis should
// recursively call getTimestamp for instructions that can be determined
// statically. When the timestamp must be computed at runtime, a call to
// requireValTimestampBeforeUser should be placed. This will cause timestamps to be
// completely lazy and only computed when needed.
llvm::Instruction& TimestampPlacer::requireValTimestampBeforeUser(llvm::Value& value, llvm::Instruction& user)
{
    Timestamps::iterator it = timestamps.find(&value);
    CallInst* ci;
    if(it == timestamps.end())
    {
        const Timestamp& ts = _timestampAnalysis.getTimestamp(&value);
        InstructionToLogFunctionConverter converter(*_func.getParent(), _instIds);
        ci = converter(&value, ts);
        Value* pval = &value;
        timestamps.insert(pval, new PlacedTimestamp(ts, ci));

        // Bind the instruction to the terminator of the block it was
        // generated. This preseves control dependencies.
        Instruction* inst = dyn_cast<Instruction>(pval);
        if(inst)
            constrainInstPlacement(*ci, *inst->getParent()->getTerminator());
    }
    else
        ci = it->second->ci;

    constrainInstPlacement(*ci, user);
    return *ci;
}

/**
 * Instruments the function.
 */
void TimestampPlacer::insertInstrumentation()
{
    foreach(BasicBlock& bb, _func)
    {
		// Don't instrument landing pads or resume blocks
		if (bb.isLandingPad() 
			|| dyn_cast<ResumeInst>(bb.getTerminator()) != NULL) {
			LOG_DEBUG() << "skipping landingpad/resume block\n";
			LOG_DEBUG() << bb << "\n";
			continue;
		}

        if(_basicBlockSignal)
            (*_basicBlockSignal)(bb);

        foreach(Instruction& inst, bb)
        {
		LOG_DEBUG() << "inserting instrumentation for: " << inst << "\n";
            Signals::iterator it = _signals.find(inst.getOpcode());
            if(it != _signals.end())

                // Call the signal with the instruction.
                (*it->second)(inst);
        }
    }

    _placer.placeInsts();
}

#ifndef INSTRUMENTATION_CALL_H
#define INSTRUMENTATION_CALL_H

#include <llvm/Instruction.h>
#include <llvm/Instructions.h>

// TODO: Replace all insrumentation calls with this class.
// TODO: Rename
class InstrumentationCall
{
    public:
	enum InsertLocation
	{
		INSERT_BEFORE,
		INSERT_AFTER
	};

    private:
    llvm::Instruction* generatedFrom;
	llvm::Instruction* insertTarget;
	InsertLocation insertLocation;
    llvm::CallInst* instrumentationCall;

	public:
	InstrumentationCall(llvm::CallInst* instrumentationCall, llvm::Instruction* insertTarget, InsertLocation insertLocation, llvm::Instruction* generatedFrom);
    virtual ~InstrumentationCall();

    virtual void instrument() = 0;
};

#endif // INSTRUMENTATION_CALL_H

#ifndef INSTRUMENTATION_CALL_H
#define INSTRUMENTATION_CALL_H

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>

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

	public:
	InstrumentationCall(llvm::CallInst* instrumentationCall, 
        llvm::Instruction* insertTarget, 
        InsertLocation insertLocation, 
        llvm::Instruction* generatedFrom);

    virtual ~InstrumentationCall();

    virtual void instrument() = 0;

    protected:
    llvm::Instruction* generatedFrom;
	llvm::Instruction* insertTarget;
	InsertLocation insertLocation;
    llvm::CallInst* instrumentationCall;

};

#endif // INSTRUMENTATION_CALL_H

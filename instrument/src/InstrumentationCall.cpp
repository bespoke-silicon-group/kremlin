#include "InstrumentationCall.h"

InstrumentationCall::InstrumentationCall(llvm::CallInst* instrumentationCall, llvm::Instruction* insertTarget, InsertLocation insertLocation, llvm::Instruction* generatedFrom) :
    generatedFrom(generatedFrom),
    insertTarget(insertTarget),
    insertLocation(insertLocation),
    instrumentationCall(instrumentationCall)
{
}

InstrumentationCall::~InstrumentationCall()
{
}

#ifndef INSTRUMENTATION_CALL_H
#define INSTRUMENTATION_CALL_H

// TODO: Replace all insrumentation calls with this class.
class InstrumentationCall
{
	enum InsertLocation
	{
		INSERT_BEFORE,
		INSERT_AFTER
	};

	Instruction* generatedFrom;
	Instruction* insertTarget;
	InsertLocation insertLocation;
	CallInst* instrumentationCall;

	public:
	InstrumentationCall(CallInst* instrumentationCall, Instruction* insertTarget, InsertLocation insertLocation, Instruction* generatedFrom);
};

#endif // INSTRUMENTATION_CALL_H

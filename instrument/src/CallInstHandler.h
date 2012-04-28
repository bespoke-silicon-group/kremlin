#ifndef CALL_INST_HANDLER
#define CALL_INST_HANDLER

#include <stdint.h>
#include <vector>
#include "TimestampPlacerHandler.h"
#include "TimestampPlacer.h"
#include "PassLog.h"

class CallInstHandler : public TimestampPlacerHandler
{
    public:
    CallInstHandler(TimestampPlacer& timestamp_placer);
    virtual ~CallInstHandler() {};

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    template <typename Callable>
    static llvm::Function* untangleCall(Callable& callable_inst);

	void addIgnore(std::string func_name);

    private:
    uint64_t callSiteIdCounter;
    PassLog& log;
    llvm::Function* linkArgConstFunc;
    llvm::Function* linkArgFunc;
    Opcodes opcodesOfHandledInsts;
	std::vector<std::string> ignoredFuncs;
    llvm::Function* prepCallFunc;
    llvm::Function* linkReturnFunc;
    TimestampPlacer& timestampPlacer;

	bool shouldHandle(llvm::Function *func);
};

#endif // CALL_INST_HANDLER

#ifndef CALLABLE_HANDLER
#define CALLABLE_HANDLER

#include <stdint.h>
#include <vector>
#include "TimestampPlacerHandler.h"
#include "TimestampPlacer.h"
#include "PassLog.h"

template <class Callable>
class CallableHandler : public TimestampPlacerHandler
{
    public:
    CallableHandler(TimestampPlacer& timestamp_placer);
    virtual ~CallableHandler() {};

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    //template <typename Callable>
    static llvm::Function* untangleCall(Callable& callable_inst);

	void addIgnore(std::string func_name);
	void addIgnore(std::vector<std::string>& func_names);
	void addOpcode(unsigned opcode);

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

#include "CallableHandler.tcc"

#endif // CALLABLE_HANDLER

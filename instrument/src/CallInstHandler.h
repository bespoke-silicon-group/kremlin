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
    CallInstHandler(TimestampPlacer& ts_placer);
    virtual ~CallInstHandler() {};

    virtual const Opcodes& getOpcodes();
    virtual void handle(llvm::Instruction& inst);

    template <typename Callable>
    static llvm::Function* untangleCall(Callable& ci);

	void addIgnore(std::string func_name);

    private:
    uint64_t call_idx;
    PassLog& log;
    llvm::Function* link_arg_const_func;
    llvm::Function* link_arg_func;
    Opcodes opcodes;
	std::vector<std::string> ignored_funcs;
    llvm::Function* prepare_call_func;
    llvm::Function* ret_val_link_func;
    TimestampPlacer& ts_placer;

	bool shouldHandle(llvm::Function *func);
};

#endif // CALL_INST_HANDLER

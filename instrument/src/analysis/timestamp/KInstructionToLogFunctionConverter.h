#ifndef TIMESTAMP_TO_LOG_FUNCTION_CONVERTER_H
#define TIMESTAMP_TO_LOG_FUNCTION_CONVERTER_H

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <map>

#include <boost/utility.hpp>

#include "PassLog.h"
#include "analysis/timestamp/Timestamp.h"
#include "ids/InstIds.h"

class InstructionToLogFunctionConverter : boost::noncopyable
{
    public:
    typedef const std::map<const llvm::Value*, unsigned> Ids; 

    public:
    InstructionToLogFunctionConverter(llvm::Module& m, InstIds& inst_to_id);
    ~InstructionToLogFunctionConverter();

    llvm::CallInst* operator()(const llvm::Value* inst, const Timestamp& ts) const;

    private:
    typedef std::map<size_t, llvm::Function*> FuncMap;

    InstIds& inst_to_id;
    PassLog& log;
    llvm::Function* log_func;
    llvm::Module& m;
    FuncMap func_map;
};

#endif // TIMESTAMP_TO_LOG_FUNCTION_CONVERTER_H

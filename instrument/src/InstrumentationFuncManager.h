#ifndef INSTRUMENTATION_FUNC_MANAGER_H
#define INSTRUMENTATION_FUNC_MANAGER_H

// TODO: Rename to InstrumentationFuncs.{h,cpp}
// TODO: Make this a struct of all the Function const* we can add

#include <llvm/Function.h>
#include <llvm/Module.h>
#include <map>
#include "PassLog.h"

class InstrumentationFuncManager
{
	std::map<std::string, llvm::Function*> instrumentation_funcs;
	PassLog& log;
	llvm::Module& module;

	void addFunc(const std::string& name, llvm::FunctionType* type);

	public:
	InstrumentationFuncManager(llvm::Module& module);
	virtual ~InstrumentationFuncManager();

	void initializeDefaultValues();

	const llvm::Type* getArgTypeFromFirstUser(llvm::Function* func, unsigned arg_no);

	llvm::Function* get(const std::string& name);
};

#endif // INSTRUMENTATION_FUNC_MANAGER_H

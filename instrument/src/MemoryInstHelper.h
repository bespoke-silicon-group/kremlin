#ifndef MEMORY_INST_HELPER_H
#define MEMORY_INST_HELPER_H

#include <llvm/IR/Value.h>
#include "PassLog.h"

/**
 * A helper for memory instructions (i.e. load/store)
 */
class MemoryInstHelper
{
	public:
	static unsigned getTypeSizeInBytes(llvm::Value *val);

	private:
	MemoryInstHelper() : log(PassLog::get()) { }

	PassLog& log;
};


#endif // MEMORY_INST_HELPER_H

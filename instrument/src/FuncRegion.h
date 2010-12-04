#ifndef FUNC_REGION_H
#define FUNC_REGION_H

#include <llvm/Function.h>
#include "Region.h"

class FuncRegion : public Region
{
	private:
	llvm::Function* function;
	RegionId id;

	public:
	FuncRegion(RegionId id, llvm::Function* func);
};

#endif // FUNC_REGION_H

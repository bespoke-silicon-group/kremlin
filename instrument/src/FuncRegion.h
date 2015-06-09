#ifndef FUNC_REGION_H
#define FUNC_REGION_H

#include <llvm/IR/Function.h>
#include <iostream>
#include "Region.h"

class FuncRegion : public Region
{
	private:
	llvm::Function* func;
	RegionId id;
	std::string funcName;
	std::string fileName;
	unsigned int startLine;

	public:
	static const std::string REGION_NAME;

	FuncRegion(RegionId id, llvm::Function* func);
	virtual ~FuncRegion();

	virtual RegionId getId() const;
	virtual const std::string& getRegionType() const;
	virtual const std::string& getFileName() const;
	virtual const std::string& getFuncName() const;
	virtual unsigned int getEndLine() const;
	virtual unsigned int getStartLine() const;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const FuncRegion& r);

#endif // FUNC_REGION_H

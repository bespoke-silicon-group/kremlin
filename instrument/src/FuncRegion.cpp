#include "llvm/IR/DebugInfo.h"
#include <llvm/IR/Metadata.h>
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include "FuncRegion.h"
#include "UnsupportedOperationException.h"

using namespace llvm;

const std::string FuncRegion::REGION_NAME = "func";

FuncRegion::FuncRegion(RegionId id, llvm::Function* func) : 
	func(func),
	id(id)
{
	DebugInfoFinder debugInfoFinder;
	debugInfoFinder.processModule(*func->getParent());

	for(DebugInfoFinder::subprogram_iterator it = debugInfoFinder.subprograms().begin(), end = debugInfoFinder.subprograms().end(); it != end; it++)
	{
		const DISubprogram *dis = it;
		Function *f = dis->getFunction();
		if(f == func)
		{
			std::string rawName = dis->getFilename().str();
			size_t substr_start = rawName.rfind('/');
			if (substr_start == std::string::npos) {
				substr_start = 0;
			}
			else {
				substr_start++;
			}
			fileName = rawName.substr(substr_start);
			funcName = dis->getDisplayName().str();
			if (funcName.length() > 1 && funcName.at(0) == '~') funcName.replace(0, 1, "destructor_");
			assert(funcName.find('/') == std::string::npos);
			startLine = dis->getLineNumber();
		}
	}

	if(fileName == "")
	{
		std::string rawName = debugInfoFinder.compile_units().begin()->getFilename();
		size_t substr_start = rawName.rfind('/');
		if (substr_start == std::string::npos) {
			substr_start = 0;
		}
		else {
			substr_start++;
		}
		fileName = rawName.substr(substr_start);
	}
}

FuncRegion::~FuncRegion()
{
}

RegionId FuncRegion::getId() const
{
	return id;
}

const std::string& FuncRegion::getFileName() const
{
	return fileName;
}

const std::string& FuncRegion::getFuncName() const
{
	return funcName;
}

const std::string& FuncRegion::getRegionType() const
{
	return REGION_NAME;
}

unsigned int FuncRegion::getStartLine() const
{
	return startLine;
}

unsigned int FuncRegion::getEndLine() const
{
	return startLine;
}


llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const FuncRegion& r)
{
	os << "FuncRegion(id: " << r.getId() 
		<< ", name: " << r.getFuncName()
		<< ", fileName: " << r.getFileName()
		<< ", startLine: " << r.getStartLine()
		<< ", endLine: " << r.getEndLine();

	return os;
}

#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Metadata.h>
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include "FuncRegion.h"
#include "SubprogramDebugInfo.h"
#include "CompileUnitDebugInfo.h"
#include "UnsupportedOperationException.h"

using namespace llvm;

const std::string FuncRegion::REGION_NAME = "func";

FuncRegion::FuncRegion(RegionId id, llvm::Function* func) : 
	func(func),
	id(id)
{
	DebugInfoFinder debugInfoFinder;
	debugInfoFinder.processModule(*func->getParent());

	for(DebugInfoFinder::iterator it = debugInfoFinder.subprogram_begin(), end = debugInfoFinder.subprogram_end(); it != end; it++)
	{
		SubprogramDebugInfo debugInfo(*it);

		if(debugInfo.func == func)
		{
			fileName = debugInfo.fileName;
			funcName = debugInfo.displayName;
			startLine = debugInfo.lineNumber;
		}
	}

	if(fileName == "")
	{
		CompileUnitDebugInfo compilationDebugInfo(*debugInfoFinder.compile_unit_begin());
		fileName = compilationDebugInfo.fileName;
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

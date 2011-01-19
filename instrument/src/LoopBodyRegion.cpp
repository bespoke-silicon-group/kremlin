#include <algorithm>
#include <iostream>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include "CompileUnitDebugInfo.h"
#include "DebugInfoParser.h"
#include "LoopBodyRegion.h"
#include "PassLog.h"
#include "SubprogramDebugInfo.h"
#include "UnsupportedOperationException.h"
#include "foreach.h"

using namespace llvm;

typedef std::pair<unsigned, MDNode*> AllMetaType;

const std::string LoopBodyRegion::REGION_NAME = "loop_body";

LoopBodyRegion::LoopBodyRegion(RegionId id, llvm::Loop* loop) : 
	loop(loop),
	id(id)
{
	Function* func = (*loop->block_begin())->getParent();
	DebugInfoFinder debugInfoFinder;
	debugInfoFinder.processModule(*func->getParent());

	for(DebugInfoFinder::iterator it = debugInfoFinder.subprogram_begin(), end = debugInfoFinder.subprogram_end(); it != end; it++)
	{
		SubprogramDebugInfo debugInfo(*it);
		if(debugInfo.func == func)
		{
			fileName = debugInfo.fileName;
			funcName = debugInfo.displayName;
		}
	}

	// Subprogram information doesn't have file name information?
	// Consequently, we just fetch it from the compilation information debug
	// info.
	if(fileName == "")
	{
		CompileUnitDebugInfo compilationDebugInfo(*debugInfoFinder.compile_unit_begin());
		fileName = compilationDebugInfo.fileName;
	}

	LOG_DEBUG() << "Meta data for " << id << "\n";

	// Get the line numbers from the set of instructions.
	startLine = UINT_MAX;
	endLine = 0;
	foreach(BasicBlock* bb, loop->getBlocks())
		foreach(Instruction& inst, *bb)
		{
			if (MDNode *N = inst.getMetadata("dbg")) {  // grab debug metadata from inst
				DILocation Loc(N);                      // get location info from metadata
				unsigned line_no = Loc.getLineNumber();

				startLine = std::min(startLine,line_no);
				endLine = std::max(endLine,line_no);
			}
		}
}

LoopBodyRegion::~LoopBodyRegion()
{
}

RegionId LoopBodyRegion::getId() const
{
	return id;
}

const std::string& LoopBodyRegion::getFileName() const
{
	return fileName;
}

const std::string& LoopBodyRegion::getFuncName() const
{
	return funcName;
}

const std::string& LoopBodyRegion::getRegionType() const
{
	return REGION_NAME;
}

unsigned int LoopBodyRegion::getStartLine() const
{
	return startLine;
}

unsigned int LoopBodyRegion::getEndLine() const
{
	return endLine;
}

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const LoopBodyRegion& r)
{
	os << "LoopBodyRegion(id: " << r.getId() 
		<< ", name: " << r.getFuncName()
		<< ", fileName: " << r.getFileName()
		<< ", startLine: " << r.getStartLine()
		<< ", endLine: " << r.getEndLine();

	return os;
}

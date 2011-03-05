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
    uint64_t funcStartLine = ULLONG_MAX;
    uint64_t funcEndLine = ULLONG_MAX;
	debugInfoFinder.processModule(*func->getParent());

	for(DebugInfoFinder::iterator it = debugInfoFinder.subprogram_begin(), end = debugInfoFinder.subprogram_end(); it != end; it++)
	{
		SubprogramDebugInfo debugInfo(*it);
		if(debugInfo.func == func)
		{
			fileName = debugInfo.fileName;
			funcName = debugInfo.displayName;
            funcStartLine = debugInfo.lineNumber;
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

    // Look for the next function's start line number. This will be our
    // function's end line number.
    for(DebugInfoFinder::iterator it = debugInfoFinder.subprogram_begin(), end = debugInfoFinder.subprogram_end(); it != end; it++)
    {
        SubprogramDebugInfo debugInfo(*it);
        if(debugInfo.func != func && debugInfo.lineNumber >= funcStartLine)
        {
            funcEndLine = std::min(funcEndLine, (uint64_t)debugInfo.lineNumber);
        }
    }

	LOG_DEBUG() << "Meta data for " << id << "\n";

	// Get the line numbers from the set of instructions.
	startLine = UINT_MAX;
	endLine = 0;
	foreach(BasicBlock* bb, loop->getBlocks())
		foreach(Instruction& inst, *bb)
		{
			if (MDNode *N = inst.getMetadata("dbg")) {  // grab debug metadata from inst
				DILocation loc(N);                      // get location info from metadata

				unsigned line_no = loc.getLineNumber();

                // Only update if within bounds of our function
                if(line_no >= funcStartLine && line_no <= funcEndLine)
                {
                    startLine = std::min(startLine,line_no);
                    endLine = std::max(endLine,line_no);
                }
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

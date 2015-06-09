#include <algorithm>
#include <iostream>
#include "llvm/IR/DebugInfo.h"
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include "LoopBodyRegion.h"
#include "PassLog.h"
#include "UnsupportedOperationException.h"
#include "foreach.h"

using namespace llvm;

typedef std::pair<unsigned, MDNode*> AllMetaType;

const std::string LoopBodyRegion::REGION_NAME = "loop_body";

// TODO: refactor so there isn't a ton of code duplication between this and LoopRegion.cpp
LoopBodyRegion::LoopBodyRegion(RegionId id, llvm::Loop* loop) : 
	loop(loop),
	id(id)
{
	Function* func = (*loop->block_begin())->getParent();
	DebugInfoFinder debugInfoFinder;
    uint64_t funcStartLine = ULLONG_MAX;
    uint64_t funcEndLine = ULLONG_MAX;
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
			assert(fileName.find('/') == std::string::npos);
			funcName = dis->getDisplayName().str();
			startLine = dis->getLineNumber();
		}
	}

	// Subprogram information doesn't have file name information?
	// Consequently, we just fetch it from the compilation information debug
	// info.
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

	assert(funcName.length() > 0);

    // Look for the next function's start line number. This will be our
    // function's end line number.
    for(DebugInfoFinder::subprogram_iterator it = debugInfoFinder.subprograms().begin(), end = debugInfoFinder.subprograms().end(); it != end; it++)
    {
		const DISubprogram *dis = it;
		Function *f = dis->getFunction();
        if(f != func && dis->getLineNumber() >= funcStartLine)
        {
            funcEndLine = std::min(funcEndLine, 
									(uint64_t)dis->getLineNumber());
        }
    }

	LOG_DEBUG() << "Meta data for " << id << "\n";

	// Get the line numbers from the set of instructions.
    unsigned int startLine_candidate = startLine = UINT_MAX;
    unsigned int endLine_candidate = endLine = 0;
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

                startLine_candidate = std::min(startLine_candidate,line_no);
                endLine_candidate = std::max(endLine_candidate,line_no);
			}
		}

	// If we haven't set the startLine it means we have an inlined loop
	// whose "true" containing function was not nested inside a loop in
	// the function it was inlined into. In this case, we just use the
	// line numbers from the "true" containing function.
	if(startLine == UINT_MAX) {
		startLine = startLine_candidate;
		endLine = endLine_candidate;
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

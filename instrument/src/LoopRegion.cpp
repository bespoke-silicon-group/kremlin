#include <algorithm>
#include <iostream>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include "CompileUnitDebugInfo.h"
#include "DebugInfoParser.h"
#include "LoopRegion.h"
#include "SubprogramDebugInfo.h"
#include "UnsupportedOperationException.h"
#include "foreach.h"

using namespace llvm;

typedef std::pair<unsigned, MDNode*> AllMetaType;

const std::string LoopRegion::REGION_NAME = "loop";

LoopRegion::LoopRegion(RegionId id, llvm::Loop* loop) : 
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

	std::cerr << "Meta data for " << id << std::endl;

	// Get the line numbers from the set of instructions.
	// XXX: COMPLETE HAX
	startLine = UINT_MAX;
	endLine = 0;
	foreach(BasicBlock* bb, loop->getBlocks())
		foreach(Instruction& inst, *bb)
		{
			// Line information only can be grabbed from all data and it's the
			// first argument?? This is not what the documentation says...
			SmallVectorImpl<AllMetaType> metadata(256);
			inst.getAllMetadata(metadata);
			foreach(AllMetaType& p, metadata)
			{
				p.second->dump();

				startLine = std::min(startLine, (unsigned int)DebugInfoParser::parseInt(p.second->getOperand(0)));
				endLine = std::max(endLine, (unsigned int)DebugInfoParser::parseInt(p.second->getOperand(0)));
			}


			/*
			 * This should work if the documentation was correct...
			startLine = std::min(startLine, inst.getDebugLoc().getLine());
			endLine = std::max(endLine, inst.getDebugLoc().getLine());
			*/
		}
}

LoopRegion::~LoopRegion()
{
}

RegionId LoopRegion::getId() const
{
	return id;
}

const std::string& LoopRegion::getFileName() const
{
	return fileName;
}

const std::string& LoopRegion::getFuncName() const
{
	return funcName;
}

const std::string& LoopRegion::getRegionType() const
{
	return REGION_NAME;
}

unsigned int LoopRegion::getStartLine() const
{
	return startLine;
}

unsigned int LoopRegion::getEndLine() const
{
	return endLine;
}

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const LoopRegion& r)
{
	os << "LoopRegion(id: " << r.getId() 
		<< ", name: " << r.getFuncName()
		<< ", fileName: " << r.getFileName()
		<< ", startLine: " << r.getStartLine()
		<< ", endLine: " << r.getEndLine();

	return os;
}

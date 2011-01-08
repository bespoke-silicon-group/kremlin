#include "CompileUnitDebugInfo.h"
#include <llvm/Constants.h>
#include "DebugInfoParser.h"

using namespace llvm;

CompileUnitDebugInfo::CompileUnitDebugInfo(MDNode* compileUnitMetaData) :
	debugInfoVersion((int)DebugInfoParser::parseInt(compileUnitMetaData->getOperand(MDI_DEBUG_INFO_VERSION))),
	unusedField((int)DebugInfoParser::parseInt(compileUnitMetaData->getOperand(MDI_UNUSED))),
	language(DebugInfoParser::parseString(compileUnitMetaData->getOperand(MDI_LANGUAGE))),
	fileName(DebugInfoParser::parseString(compileUnitMetaData->getOperand(MDI_FILE_NAME))),
	directory(DebugInfoParser::parseString(compileUnitMetaData->getOperand(MDI_DIRECTORY))),
	producer(DebugInfoParser::parseString(compileUnitMetaData->getOperand(MDI_PRODUCER))),
	isMain((bool)DebugInfoParser::parseInt(compileUnitMetaData->getOperand(MDI_IS_MAIN))),
	isOptimized((bool)DebugInfoParser::parseInt(compileUnitMetaData->getOperand(MDI_IS_OPTIMIZED))),
	flags(DebugInfoParser::parseString(compileUnitMetaData->getOperand(MDI_FLAGS))),
	version((int)DebugInfoParser::parseInt(compileUnitMetaData->getOperand(MDI_VERSION)))
{
}

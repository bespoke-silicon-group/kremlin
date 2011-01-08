#include "SubprogramDebugInfo.h"
#include <llvm/Constants.h>
#include "DebugInfoParser.h"

using namespace llvm;

SubprogramDebugInfo::SubprogramDebugInfo(MDNode* subprogramMetaData) :
	debugInfoVersion((int)DebugInfoParser::parseInt(subprogramMetaData->getOperand(MDI_DEBUG_INFO_VERSION))),
	unusedField((int)DebugInfoParser::parseInt(subprogramMetaData->getOperand(MDI_UNUSED))),
	name(DebugInfoParser::parseString(subprogramMetaData->getOperand(MDI_NAME))),
	displayName(DebugInfoParser::parseString(subprogramMetaData->getOperand(MDI_DISPLAY_NAME))),
	mipsLinkageName(DebugInfoParser::parseString(subprogramMetaData->getOperand(MDI_MIPS_LINKAGE_NAME))),
	fileName(DebugInfoParser::parseString(subprogramMetaData->getOperand(MDI_FILE))),
	lineNumber((int)DebugInfoParser::parseInt(subprogramMetaData->getOperand(MDI_LINE_NUMBER))),
	func(cast<Function>(subprogramMetaData->getOperand(MDI_FUNC_PTR)))
{
}

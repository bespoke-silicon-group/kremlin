#include "DebugInfoParser.h"

using namespace llvm;

int64_t DebugInfoParser::parseInt(llvm::Value* metadata)
{
	ConstantInt* i;
	if((i = dyn_cast<ConstantInt>(metadata)) == NULL)
		return -1;

	return i->getValue().getSExtValue();
}

std::string DebugInfoParser::parseString(llvm::Value* metadata)
{
	MDString* s;
	if((s = dyn_cast<MDString>(metadata)) == NULL)
		return "";

	return s->getString().str();
}


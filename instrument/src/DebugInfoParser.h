#ifndef DEBUG_INFO_PARSER_H
#define DEBUG_INFO_PARSER_H

#include <llvm/Metadata.h>
#include <llvm/Value.h>
#include <llvm/Constants.h>

class DebugInfoParser
{
	public:

	/**
	 * Parses a possible ConstantInt to an int64.
	 *
	 * @return the integer or -1 if the metadata is null.
	 */
	static int64_t parseInt(llvm::Value* metadata);

	/**
	 * Parses a possible MDString to a std::string.
	 *
	 * @return the string or the empty string if the meta data is null.
	 */
	static std::string parseString(llvm::Value* metadata);
};

#endif // DEBUG_INFO_PARSER_H

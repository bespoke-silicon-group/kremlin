#ifndef COMPILE_UNIT_DEBUG_INFO
#define COMPILE_UNIT_DEBUG_INFO

#include <llvm/Function.h>
#include <llvm/Metadata.h>

struct CompileUnitDebugInfo
{
	const int debugInfoVersion;
	const int unusedField;
	const std::string language;
	const std::string fileName;
	const std::string directory;
	const std::string producer;
	const bool isMain;
	const bool isOptimized;
	const std::string flags;
	const int version;

	CompileUnitDebugInfo(llvm::MDNode* compileUnitMetaData);

	/**
	 * Magic numbers for subprogram debug meta data. 
	 *
	 * See http://llvm.org/docs/SourceLevelDebugging.html#format_compile_unit
	 */
	enum MetadataIndex
	{
		MDI_DEBUG_INFO_VERSION,
		MDI_UNUSED,
		MDI_LANGUAGE,
		MDI_FILE_NAME,
		MDI_DIRECTORY,
		MDI_PRODUCER,
		MDI_IS_MAIN,
		MDI_IS_OPTIMIZED,
		MDI_FLAGS,
		MDI_VERSION
	};
};

#endif // COMPILE_UNIT_DEBUG_INFO

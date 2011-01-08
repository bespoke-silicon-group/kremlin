#ifndef SUBPROGRAM_DEBUG_INFO
#define SUBPROGRAM_DEBUG_INFO

#include <llvm/Function.h>
#include <llvm/Metadata.h>

struct SubprogramDebugInfo
{
	const int debugInfoVersion;
	const int unusedField;
	const std::string name;
	const std::string displayName;
	const std::string mipsLinkageName;
	const std::string fileName;
	const int lineNumber;
	const llvm::Function* func;

	SubprogramDebugInfo(llvm::MDNode* subprogramMetaData);

	/**
	 * Magic numbers for subprogram debug meta data. 
	 *
	 * See http://llvm.org/docs/SourceLevelDebugging.html#format_subprograms
	 */
	enum MetadataIndex
	{
		MDI_DEBUG_INFO_VERSION,
		MDI_UNUSED,
		MDI_CONTEXT_DESCRIPTOR,
		MDI_NAME,
		MDI_DISPLAY_NAME,
		MDI_MIPS_LINKAGE_NAME,
		MDI_FILE,
		MDI_LINE_NUMBER,
		MDI_TYPE_DESCRIPTOR,
		MDI_IS_STATIC,
		MDI_IS_DEFINITION,
		MDI_VIRTUALITY,
		MDI_INDEX_INTO_VIRTUAL_FUNCTION,
		MDI_DERIVED_TYPE,
		MDI_IS_ARTIFICIAL,
		MDI_IS_OPTIMIZED,
		MDI_FUNC_PTR
	};
};

#endif // SUBPROGRAM_DEBUG_INFO

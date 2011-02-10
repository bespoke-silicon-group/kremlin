# ---------------------------------------------------------------------------
# Compiles from C to assembly.
# ---------------------------------------------------------------------------

ifndef COMPILE_MK
COMPILE_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/llvmGenericRules.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/llvmRules.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/useTemp.mk

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

# Anything we must instrument.
# Unprocessed sources are files with source code (as opposed to e.g. object files)
UNPROCESSED_SOURCES += $(filter %.c %.cc %.cxx %.C %.cpp %.f95 %.f, $(SOURCES))

# All the unprocessed sources without the .c
UNPROCESSED_SOURCES_NO_EXTENSION = $(basename $(UNPROCESSED_SOURCES))

# Instrumented byte code
ASM_INSTRUMENTED = $(addsuffix $(PASS_CHAIN).bc.s, $(UNPROCESSED_SOURCES_NO_EXTENSION))
ASM_INSTRUMENTED_WITH_GCC_NAME = $(addsuffix .s, $(UNPROCESSED_SOURCES_NO_EXTENSION))

# Passes required as a chained rule. The code to instrument must go through
# all these passes.
PASS_CHAIN = .simplifycfg.mem2reg.indvars.elimsinglephis.O2.splitbbatfunccall.criticalpath.regioninstrument

# ---------------------------------------------------------------------------
# Functions (alpha order)
# ---------------------------------------------------------------------------

# Function:          C_TO_ASM
# Description:       Returns rules for converting C files to assembly for a 
#                    single file.
#
# Parameters:        $(1)   The source file name.
#                    $(2)   The output file name.
# Return:            The makefile text that creates the rule.
define C_TO_ASM

# Files
# -----

# The source file.
$(eval SOURCE_FILE := $$(1))

# The output file.
$(eval OUTPUT_FILE := $$(2))

# The base name of the source file.
$(eval SOURCE_BASE := $$(basename $(SOURCE_FILE)))

# The source file as llvm byte code.
$(eval SOURCE_BC := $$(addsuffix .bc, $(SOURCE_BASE)))

# The source after all the instrumentation passes.
$(eval ASM_INSTRUMENTED := $$(addsuffix $(PASS_CHAIN).bc.s, $(SOURCE_BASE)))

# Rules (alpha order)
# -------------------

$$(info C_TO_ASM: SOURCE_FILE: $(SOURCE_FILE) OUTPUT_FILE: $(OUTPUT_FILE))

# GCC always overwrites its output files, so mark it as phony so make will
# overwrite it also.
.PHONY: $(COMPILE_OUTPUT_FILE)

# If COMPILE_OUTPUT_FILE is defined, create a rule to convert to it. This is
# specified if -o is specified.
$(COMPILE_OUTPUT_FILE): $(ASM_INSTRUMENTED)
	mv $$< $$@

# Converts the source file to LLVM byte code.
$(SOURCE_BC): $(SOURCE_FILE)
	$(LLVM_CC) $(LLVM_CFLAGS) --emit-llvm -c -o $$@ $$<

# Create a rule to compile the file from the source name to the desired output
# file name.
$(OUTPUT_FILE): $(ASM_INSTRUMENTED)
	mv $$< $$@

ASM_SOURCES += $(OUTPUT_FILE)

endef # C_TO_ASM

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------

.PHONY: compile

$(call MAKE_ALL_USING_TMP,$(UNPROCESSED_SOURCES),.s,C_TO_ASM)

# Creates all the instrumented assembly
compile: $(ASM_INSTRUMENTED_WITH_GCC_NAME)
	@echo "sources: $(SOURCES)"
	@echo "sources_c: $(UNPROCESSED_SOURCES)"
	@echo "instrumented asm: $(ASM_INSTRUMENTED)"
	@echo "desired asm: $(ASM_INSTRUMENTED_WITH_GCC_NAME)"

clean::
	-@$(RM) $(ASM_INSTRUMENTED_WITH_GCC_NAME)

endif # COMPILE_MK

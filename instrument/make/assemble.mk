# ---------------------------------------------------------------------------
# Assembles from .s to .o
# ---------------------------------------------------------------------------

ifndef ASSEMBLE_MK
ASSEMBLE_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/useTemp.mk

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# All the assembly files that have been instrumented or not.
ASM_SOURCES += $(filter %.s, $(SOURCES))

# All the object files that have been instrumented or not.
ASM_SOURCES_AS_OBJ = $(addsuffix .o, $(notdir $(basename $(ASM_SOURCES))))

# ---------------------------------------------------------------------------
# Functions
# ---------------------------------------------------------------------------

# Function:          ASM_TO_O
# Description:       Returns rules for converting assembly files to object 
#                    files for a single file.
#
# Parameters:        $(1)   The source file name.
#                    $(2)   The output file name.
# Return:            The makefile text that creates the rule.
define ASM_TO_O

# Files
# -----

# The source file.
$(eval SOURCE_FILE := $$(1))

# The output file.
$(eval OUTPUT_FILE := $$(2))

# Rules (alpha order)
# -------------------

$$(info ASM_TO_O: SOURCE_FILE: $(SOURCE_FILE) OUTPUT_FILE: $(OUTPUT_FILE) ASM_OUTPUT_FILE: $(ASM_OUTPUT_FILE))

# GCC always overwrites its output files, so mark it as phony so make will
# overwrite it also.
.PHONY: $(ASM_OUTPUT_FILE)

# If COMPILE_OUTPUT_FILE is defined, create a rule to convert to it. This is
# specified if -o is specified.
$(ASM_OUTPUT_FILE): $(OUTPUT_FILE)
	mv $$< $$@

# Create a rule to compile the file from the source name to the desired output
# file name.
$(OUTPUT_FILE): $(SOURCE_FILE)
	$(AS) $(ASFLAGS) -o $$@ $$<

OBJ_SOURCES += $(OUTPUT_FILE)

endef # C_TO_ASM

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------

.PHONY: assemble

$(call MAKE_ALL_USING_TMP,$(ASM_SOURCES),.o,ASM_TO_O)

# Creates all the instrumented assembly
assemble: $(ASM_SOURCES_AS_OBJ)

clean::
	-@$(RM) $(ASM_SOURCES_AS_OBJ)

endif # ASSEMBLE_MK
